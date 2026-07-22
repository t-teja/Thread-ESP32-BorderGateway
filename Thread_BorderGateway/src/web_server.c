#include "web_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ble_central.h"
#include "cJSON.h"
#include "captive_portal.h"
#include "device_registry.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hub_config.h"
#include "hub_led.h"
#include "hub_settings.h"
#include "mqtt_bridge.h"
#include "otbr_net.h"
#include "pairing.h"
#include "wifi_net.h"

static const char *TAG = "web";
static httpd_handle_t s_server;

static const char DASH_HTML[] =
"<!DOCTYPE html><html lang=en><head><meta charset=utf-8>\n<meta name=viewport content=\"width=device-width,initial-scale=1\">\n<title>Thread Hub</title>\n<script defer src=\"https://cdn.jsdelivr.net/npm/mermaid@10/dist/mermaid.min.js\"></script>\n<style>\n:root{--bg:#070b14;--card:rgba(18,28,48,.92);--line:rgba(120,160,220,.14);--text:#eef3ff;"
"--muted:#8b9cb8;--accent:#5b8cff;--accent2:#7c5cff;--ok:#34d399;--warn:#fbbf24;--danger:#f87171}\n*{box-sizing:border-box}body{margin:0;font-family:ui-sans-serif,system-ui,sans-serif;color:var(--text);background:radial-gradient(1000px 500px at 0% -10%,#1a2a55,transparent 50%),radial-gradient(800px 400px at 100% 0%,#2a1850,transparent 45%),var(--bg);min-height:100vh;padding:1.2rem}\nh1{font-size:1.35"
"rem;margin:0 0 .2rem}header p{color:var(--muted);margin:0 0 1rem;font-size:.9rem}\n.row{display:flex;gap:.5rem;flex-wrap:wrap;margin-bottom:1rem}\nbutton{border:0;border-radius:12px;padding:.6rem 1rem;font-weight:700;cursor:pointer;color:#fff;background:linear-gradient(135deg,var(--accent),var(--accent2))}\nbutton.secondary{background:rgba(255,255,255,.06);border:1px solid var(--line);color:var(--tex"
"t)}\nbutton.danger{background:rgba(248,113,113,.15);color:var(--danger);border:1px solid rgba(248,113,113,.3)}\n.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:1rem;margin-bottom:1rem}\n.grid{display:grid;grid-template-columns:1.1fr .9fr;gap:1rem}\n@media(max-width:900px){.grid{grid-template-columns:1fr}}\ntable{width:100%;border-collapse:collapse;font-size:.9rem}th"
",td{text-align:left;padding:.5rem .3rem;border-bottom:1px solid var(--line)}\nth{color:var(--muted)}.pill{display:inline-block;padding:.12rem .5rem;border-radius:999px;font-size:.75rem}\n.on{background:rgba(52,211,153,.12);color:var(--ok)}.off{background:rgba(251,191,36,.12);color:var(--warn)}\ninput{width:100%;padding:.55rem .7rem;border-radius:10px;border:1px solid var(--line);background:rgba(7,11,"
"20,.8);color:var(--text);margin:.25rem 0 .55rem}\nlabel{font-size:.78rem;color:var(--muted)}.cand{display:flex;justify-content:space-between;gap:.5rem;align-items:center;padding:.55rem 0;border-bottom:1px solid var(--line)}\n#toast{position:fixed;bottom:1rem;right:1rem;background:#152038;padding:.75rem 1rem;border-radius:12px;display:none;border:1px solid var(--line);max-width:280px;z-index:9}\na.lin"
"k{color:#9db7ff;font-size:.85rem}\n#mapWrap{min-height:280px;overflow:auto}.mermaid{color:var(--text)}\n</style></head><body>\n<header><h1>Thread Hub</h1><p id=status>Loading…</p>\n<p><a class=link href=/settings>Settings</a></p></header>\n<div class=row>\n<button onclick=openPair()>Add device</button>\n<button class=secondary onclick=closePair()>Close pairing</button>\n<button class=secondary onclick=ref"
"resh()>Refresh</button>\n<button class=secondary onclick=\"j('/api/identify',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cmd:'hub'})}).then(()=>toast('Hub LED identify'))\">Hub LED</button>\n</div>\n<div class=grid>\n<div class=card><h2 style=\"margin:0 0 .75rem;font-size:1rem\">Paired devices</h2><div id=devices>…</div></div>\n<div class=card><h2 style=\"margin:0 0 .75rem;font-s"
"ize:1rem\">Network map</h2>\n<p style=\"color:var(--muted);font-size:.82rem;margin:0 0 .75rem\">Thread mesh as seen by this hub. Online sensors update live via MQTT.</p>\n<div id=mapWrap><pre class=mermaid id=map>flowchart TD\n  H[Thread Hub]</pre></div></div>\n</div>\n<div class=card id=pairPanel style=display:none><h2 style=\"margin:0;font-size:1rem\">Pairing <span id=timer class=\"pill on\"></span></h2>\n<p"
" style=\"color:var(--muted);font-size:.85rem\">Hold sensor button ~3s until LED blinks fast. Single-click on paired sensor requests identify (hub LED blinks).</p>\n<div id=cands></div>\n<div id=pairForm style=display:none;margin-top:1rem>\n<label>Name</label><input id=pname placeholder=\"Kitchen TH\">\n<label>Location / room</label><input id=proom placeholder=Kitchen>\n<input type=hidden id=paddr><input ty"
"pe=hidden id=ptype>\n<div class=row><button onclick=doPair()>Pair device</button>\n<button class=secondary onclick=clearSel()>Cancel</button></div></div></div>\n<div id=toast></div>\n<script>\n\nconst $=id=>document.getElementById(id);\nfunction toast(m){const t=$('toast');t.textContent=m;t.style.display='block';setTimeout(()=>t.style.display='none',2500)}\nasync function j(u,opt){const r=await fetch(u,op"
"t);return r.json()}\nfunction renderMermaid(src){\n  const el=$('map');\n  el.removeAttribute('data-processed');\n  el.textContent=src||'flowchart TD\\\\n  H[Thread Hub]';\n  if(window.mermaid){ if(!window.__mmInit){mermaid.initialize({startOnLoad:false,theme:'dark'});window.__mmInit=1} mermaid.run({nodes:[el]}); }\n}\nasync function refresh(){\n  const s=await j('/api/status');\n  $('status').textContent=s.hub_id+' · '+s.ip+' · fw '+s.fw+' · Thread: '+s.thread+' · MQTT: '+(s.mqtt?'up':'down')+' · o"
"nline '+s.online+'/'+s.devices;\n  if(s.pair_open){$('pairPanel').style.display='block';$('timer').textContent=s.pair_left+'s';loadCands()}\n  else $('pairPanel').style.display='none';\n  const d=await j('/api/devices');\n  let h='<table><tr><th>Name</th><th>Room</th><th>Type</th><th>Reading</th><th></th></tr>';\n  (d.devices||[]).forEach(x=>{\n    let reading='—';\n    if(x.has_telem){\n      if(x.type=="
"='temp_hum') reading=(x.t!=null?x.t.toFixed(1):'?')+'°C / '+(x.h!=null?x.h.toFixed(0):'?')+'%';\n      else if(x.contact) reading=x.contact;\n    }\n    h+='<tr><td>'+(x.name||'-')+'</td><td>'+(x.room||'-')+'</td><td>'+x.type+'</td><td>'+reading+\n      '</td><td><span class=\"pill '+(x.online?'on':'off')+'\">'+(x.online?'online':'offline')+'</span> '+\n      '<button class=secondary style=\"padding:.2rem"
" .5rem;font-size:.75rem\" onclick=\"identify(\\''+x.id+'\\',\\''+(x.room||'')+'\\')\">Identify</button> '+\n      '<button class=danger style=\"padding:.2rem .5rem;font-size:.75rem\" onclick=\"delDev(\\''+x.id+'\\')\">Remove</button></td></tr>';\n  });\n  h+='</table>';\n  if(!(d.devices||[]).length) h='<p style=color:var(--muted)>No devices yet. Tap Add device.</p>';\n  $('devices').innerHTML=h;\n  const m=aw"
"ait j('/api/map');\n  if(m.mermaid) renderMermaid(m.mermaid);\n}\nasync function loadCands(){const d=await j('/api/candidates'); let h='';\n (d.candidates||[]).forEach(c=>{\n  h+='<div class=cand><div><strong>'+c.name+'</strong><br><span style=color:var(--muted);font-size:.8rem>'+c.type+' · '+c.addr+' · rssi '+c.rssi+'</span></div>'+\n  '<button class=secondary onclick=\"selectCand(\\''+c.addr+'\\',\\''+"
"c.type+'\\',\\''+c.name+'\\')\">Select</button></div>';});\n if(!h) h='<p style=color:var(--muted)>Scanning… put sensor in pair mode near the hub.</p>';\n $('cands').innerHTML=h;}\nfunction selectCand(a,t,n){$('paddr').value=a;$('ptype').value=t;$('pname').value=n||'';$('pairForm').style.display='block'}\nfunction clearSel(){$('pairForm').style.display='none'}\nasync function openPair(){await j('/api/pa"
"ir/start',{method:'POST',headers:{'Content-Type':'application/json'},body:'{\"seconds\":120}'});toast('Pairing open 120s');refresh()}\nasync function closePair(){await j('/api/pair/stop',{method:'POST'});refresh()}\nasync function doPair(){\n const body=JSON.stringify({addr:$('paddr').value,type:$('ptype').value,name:$('pname').value,room:$('proom').value});\n const r=await j('/api/pair',{method:'POST',"
"headers:{'Content-Type':'application/json'},body});\n toast(r.ok?'Paired':'Pair failed'); clearSel(); refresh();}\nasync function delDev(id){if(!confirm('Remove '+id+'?'))return; await j('/api/devices/'+id,{method:'DELETE'});refresh()}\nasync function identify(id,room){\n await j('/api/identify',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({id:id,room:room})});\n toast"
"('Identify sent — hub LED + device LED');\n}\nrefresh(); setInterval(refresh,3000);\n\n</script></body></html>";

static const char SETTINGS_HTML[] =
"<!DOCTYPE html><html><head><meta charset=utf-8><meta name=viewport content=\"width=device-width,initial-scale=1\">"
"<title>Settings</title><style>body{font-family:system-ui;background:#070b14;color:#eef3ff;padding:1.2rem}"
".card{max-width:480px;margin:auto;background:#121c30;border-radius:16px;padding:1.2rem;border:1px solid #243}"
"label{display:block;margin-top:.6rem;color:#8b9cb8;font-size:.75rem;text-transform:uppercase}"
"input{width:100%;padding:.65rem;border-radius:10px;border:1px solid #243;background:#0b1220;color:#fff}"
"button{margin-top:1rem;width:100%;padding:.75rem;border:0;border-radius:12px;font-weight:700;color:#fff;background:linear-gradient(135deg,#5b8cff,#7c5cff)}"
"a{color:#9db7ff}</style></head><body><div class=card><p><a href=/>Dashboard</a></p><h1>Settings</h1>"
"<p style=color:#8b9cb8;font-size:.85rem>Wi-Fi / MQTT. Factory reset: Settings button or hold BOOT 5s.</p>"
"<label>Wi-Fi SSID</label><input id=wifi_ssid><label>Wi-Fi password (blank=keep)</label><input id=wifi_pass type=password>"
"<label>MQTT host</label><input id=mqtt_host><label>MQTT port</label><input id=mqtt_port type=number value=1883>"
"<label>MQTT user</label><input id=mqtt_user><label>MQTT password (blank=keep)</label><input id=mqtt_pass type=password>"
"<label>Topic base</label><input id=topic_base value=home><label>Hostname</label><input id=hostname value=thread-hub>"
"<button onclick=save()>Save and reboot</button>"
"<button style=\"background:#2a1520;border:1px solid #633;margin-top:.5rem\" onclick=factory()>Factory reset</button>"
"<div id=msg></div></div><script>"
"async function boot(){const s=await(await fetch('/api/settings')).json();"
"wifi_ssid.value=s.wifi_ssid||'';mqtt_host.value=s.mqtt_host||'';mqtt_port.value=s.mqtt_port||1883;"
"mqtt_user.value=s.mqtt_user||'';topic_base.value=s.topic_base||'home';hostname.value=s.hostname||'thread-hub';}"
"async function save(){const body={wifi_ssid:wifi_ssid.value.trim(),mqtt_host:mqtt_host.value.trim(),"
"mqtt_port:Number(mqtt_port.value||1883),mqtt_user:mqtt_user.value.trim(),"
"topic_base:topic_base.value.trim()||'home',hostname:hostname.value.trim()||'thread-hub'};"
"if(wifi_pass.value)body.wifi_pass=wifi_pass.value;if(mqtt_pass.value)body.mqtt_pass=mqtt_pass.value;"
"const r=await(await fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})).json();"
"msg.textContent=r.ok?'Saved - rebooting':(r.error||'fail');if(r.ok)setTimeout(()=>fetch('/api/reboot',{method:'POST'}),500);}"
"async function factory(){if(!confirm('Erase network settings?'))return;await fetch('/api/factory_reset',{method:'POST'});}"
"boot();</script></body></html>";

static esp_err_t send_json(httpd_req_t *req, const char *json, int status)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    if (status == 404) httpd_resp_set_status(req, "404 Not Found");
    else if (status == 400) httpd_resp_set_status(req, "400 Bad Request");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}
static esp_err_t send_html(httpd_req_t *req, const char *html, size_t len)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, html, len ? (ssize_t)len : HTTPD_RESP_USE_STRLEN);
}
static esp_err_t read_body(httpd_req_t *req, char *buf, size_t buflen)
{
    int total = req->content_len;
    if (total <= 0 || total >= (int)buflen) return ESP_ERR_INVALID_SIZE;
    int r = httpd_req_recv(req, buf, total);
    if (r <= 0) return ESP_FAIL;
    buf[r] = 0;
    return ESP_OK;
}
static esp_err_t h_favicon(httpd_req_t *req)
{ httpd_resp_set_status(req, "204 No Content"); return httpd_resp_send(req, NULL, 0); }
static esp_err_t h_root(httpd_req_t *req)
{
    if (wifi_net_is_ap_mode()) return send_html(req, captive_portal_html(), captive_portal_html_len());
    return send_html(req, DASH_HTML, 0);
}
static esp_err_t h_captive_redirect(httpd_req_t *req)
{
    if (wifi_net_is_ap_mode()) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
        return httpd_resp_send(req, NULL, 0);
    }
    return h_root(req);
}
static esp_err_t h_generate_204(httpd_req_t *req)
{
    if (wifi_net_is_ap_mode()) return h_captive_redirect(req);
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}
static esp_err_t h_settings_page(httpd_req_t *req)
{
    if (wifi_net_is_ap_mode()) return h_root(req);
    return send_html(req, SETTINGS_HTML, 0);
}
static esp_err_t h_api_settings_get(httpd_req_t *req)
{
    char *j = hub_settings_to_json_public();
    if (!j) return send_json(req, "{}", 200);
    esp_err_t e = send_json(req, j, 200); free(j); return e;
}
static esp_err_t h_api_settings_post(httpd_req_t *req)
{
    char body[768] = {0};
    if (read_body(req, body, sizeof(body)) != ESP_OK) return send_json(req, "{\"ok\":false}", 400);
    cJSON *j = cJSON_Parse(body);
    if (!j) return send_json(req, "{\"ok\":false}", 400);
    hub_settings_t s = *hub_settings_get();
    const cJSON *it;
    it = cJSON_GetObjectItem(j, "wifi_ssid"); if (cJSON_IsString(it)) strncpy(s.wifi_ssid, it->valuestring, sizeof(s.wifi_ssid)-1);
    it = cJSON_GetObjectItem(j, "wifi_pass"); if (cJSON_IsString(it)) strncpy(s.wifi_pass, it->valuestring, sizeof(s.wifi_pass)-1);
    it = cJSON_GetObjectItem(j, "mqtt_host"); if (cJSON_IsString(it)) strncpy(s.mqtt_host, it->valuestring, sizeof(s.mqtt_host)-1);
    it = cJSON_GetObjectItem(j, "mqtt_port"); if (cJSON_IsNumber(it)) s.mqtt_port = (uint16_t)it->valuedouble;
    it = cJSON_GetObjectItem(j, "mqtt_user"); if (cJSON_IsString(it)) strncpy(s.mqtt_user, it->valuestring, sizeof(s.mqtt_user)-1);
    it = cJSON_GetObjectItem(j, "mqtt_pass"); if (cJSON_IsString(it)) strncpy(s.mqtt_pass, it->valuestring, sizeof(s.mqtt_pass)-1);
    it = cJSON_GetObjectItem(j, "topic_base"); if (cJSON_IsString(it)) strncpy(s.topic_base, it->valuestring, sizeof(s.topic_base)-1);
    it = cJSON_GetObjectItem(j, "hostname"); if (cJSON_IsString(it)) strncpy(s.hostname, it->valuestring, sizeof(s.hostname)-1);
    cJSON_Delete(j);
    if (!s.wifi_ssid[0] || !s.mqtt_host[0]) return send_json(req, "{\"ok\":false,\"error\":\"ssid/mqtt required\"}", 400);
    if (hub_settings_save(&s) != ESP_OK) return send_json(req, "{\"ok\":false}", 400);
    return send_json(req, "{\"ok\":true}", 200);
}
static esp_err_t h_reboot(httpd_req_t *req)
{ send_json(req, "{\"ok\":true}", 200); vTaskDelay(pdMS_TO_TICKS(400)); esp_restart(); return ESP_OK; }
static esp_err_t h_factory(httpd_req_t *req)
{ hub_settings_clear(); send_json(req, "{\"ok\":true}", 200); vTaskDelay(pdMS_TO_TICKS(400)); esp_restart(); return ESP_OK; }
static esp_err_t h_status(httpd_req_t *req)
{
    char hub_id[32]; wifi_net_get_hub_id(hub_id, sizeof(hub_id));
    char buf[480];
    snprintf(buf, sizeof(buf),
        "{\"hub_id\":\"%s\",\"ip\":\"%s\",\"fw\":\"%s\",\"thread\":\"%s\",\"mqtt\":%s,"
        "\"devices\":%d,\"online\":%d,\"pair_open\":%s,\"pair_left\":%d,\"mode\":\"%s\",\"ap_ssid\":\"%s\"}",
        hub_id, wifi_net_get_ip(), HUB_FW_VERSION, otbr_net_status_text(),
        mqtt_bridge_is_connected() ? "true" : "false", registry_count(), registry_online_count(),
        pairing_is_open() ? "true" : "false", pairing_seconds_left(),
        wifi_net_is_ap_mode() ? "ap" : "sta",
        wifi_net_is_ap_mode() ? wifi_net_get_ap_ssid() : "");
    return send_json(req, buf, 200);
}
static esp_err_t h_devices(httpd_req_t *req)
{
    char *reg = registry_to_json();
    if (!reg) return send_json(req, "{\"devices\":[]}", 200);
    char *out = malloc(strlen(reg) + 32);
    sprintf(out, "{\"devices\":%s}", reg); free(reg);
    esp_err_t e = send_json(req, out, 200); free(out); return e;
}
static esp_err_t h_map(httpd_req_t *req)
{
    char hub_id[32]; wifi_net_get_hub_id(hub_id, sizeof(hub_id));
    char *m = registry_map_json(hub_id, otbr_net_status_text());
    if (!m) return send_json(req, "{}", 200);
    esp_err_t e = send_json(req, m, 200); free(m); return e;
}
static esp_err_t h_candidates(httpd_req_t *req)
{
    char *c = ble_central_candidates_json();
    if (!c) return send_json(req, "{\"candidates\":[]}", 200);
    char *out = malloc(strlen(c) + 32);
    sprintf(out, "{\"candidates\":%s}", c); free(c);
    esp_err_t e = send_json(req, out, 200); free(out); return e;
}
static esp_err_t h_pair_start(httpd_req_t *req)
{
    char body[128]={0}; int sec = HUB_PAIR_DEFAULT_SEC;
    if (req->content_len > 0 && read_body(req, body, sizeof(body)) == ESP_OK) {
        cJSON *j = cJSON_Parse(body);
        if (j) { cJSON *s = cJSON_GetObjectItem(j, "seconds"); if (cJSON_IsNumber(s)) sec = s->valueint; cJSON_Delete(j); }
    }
    pairing_open_window(sec);
    return send_json(req, "{\"ok\":true}", 200);
}
static esp_err_t h_pair_stop(httpd_req_t *req) { pairing_close_window(); return send_json(req, "{\"ok\":true}", 200); }
static esp_err_t h_pair(httpd_req_t *req)
{
    char body[512]={0};
    if (read_body(req, body, sizeof(body)) != ESP_OK) return send_json(req, "{\"ok\":false}", 400);
    cJSON *j = cJSON_Parse(body);
    if (!j) return send_json(req, "{\"ok\":false}", 400);
    const char *addr = cJSON_GetStringValue(cJSON_GetObjectItem(j, "addr"));
    const char *type = cJSON_GetStringValue(cJSON_GetObjectItem(j, "type"));
    const char *name = cJSON_GetStringValue(cJSON_GetObjectItem(j, "name"));
    const char *room = cJSON_GetStringValue(cJSON_GetObjectItem(j, "room"));
    esp_err_t err = pairing_pair_device(addr, type, name, room ? room : "");
    cJSON_Delete(j);
    if (err != ESP_OK) {
        char b[96]; snprintf(b, sizeof(b), "{\"ok\":false,\"error\":\"%s\"}", esp_err_to_name(err));
        return send_json(req, b, 400);
    }
    return send_json(req, "{\"ok\":true}", 200);
}
static esp_err_t h_del_device(httpd_req_t *req)
{
    const char *id = strrchr(req->uri, '/');
    if (!id || !id[1]) return send_json(req, "{\"ok\":false}", 404);
    registry_remove(id + 1); mqtt_bridge_publish_registry();
    return send_json(req, "{\"ok\":true}", 200);
}
static esp_err_t h_identify(httpd_req_t *req)
{
    char body[256]={0};
    if (req->content_len > 0) read_body(req, body, sizeof(body));
    cJSON *j = cJSON_Parse(body[0] ? body : "{}");
    const char *id = j ? cJSON_GetStringValue(cJSON_GetObjectItem(j, "id")) : NULL;
    const char *room = j ? cJSON_GetStringValue(cJSON_GetObjectItem(j, "room")) : NULL;
    /* Always blink hub LED */
    hub_led_identify(5);
    if (id && id[0]) {
        mqtt_bridge_publish_device_cmd(room ? room : "", id, "{\"cmd\":\"identify\"}");
    }
    if (j) cJSON_Delete(j);
    return send_json(req, "{\"ok\":true}", 200);
}

esp_err_t web_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = HUB_HTTP_PORT;
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    cfg.max_uri_handlers = 28;
    cfg.stack_size = 12288;
    cfg.lru_purge_enable = true;
    if (httpd_start(&s_server, &cfg) != ESP_OK) return ESP_FAIL;
    const httpd_uri_t routes[] = {
        {.uri="/", .method=HTTP_GET, .handler=h_root},
        {.uri="/favicon.ico", .method=HTTP_GET, .handler=h_favicon},
        {.uri="/settings", .method=HTTP_GET, .handler=h_settings_page},
        {.uri="/api/status", .method=HTTP_GET, .handler=h_status},
        {.uri="/api/map", .method=HTTP_GET, .handler=h_map},
        {.uri="/api/settings", .method=HTTP_GET, .handler=h_api_settings_get},
        {.uri="/api/settings", .method=HTTP_POST, .handler=h_api_settings_post},
        {.uri="/api/reboot", .method=HTTP_POST, .handler=h_reboot},
        {.uri="/api/factory_reset", .method=HTTP_POST, .handler=h_factory},
        {.uri="/api/devices", .method=HTTP_GET, .handler=h_devices},
        {.uri="/api/candidates", .method=HTTP_GET, .handler=h_candidates},
        {.uri="/api/pair/start", .method=HTTP_POST, .handler=h_pair_start},
        {.uri="/api/pair/stop", .method=HTTP_POST, .handler=h_pair_stop},
        {.uri="/api/pair", .method=HTTP_POST, .handler=h_pair},
        {.uri="/api/identify", .method=HTTP_POST, .handler=h_identify},
        {.uri="/api/devices/*", .method=HTTP_DELETE, .handler=h_del_device},
        {.uri="/generate_204", .method=HTTP_GET, .handler=h_generate_204},
        {.uri="/gen_204", .method=HTTP_GET, .handler=h_generate_204},
        {.uri="/hotspot-detect.html", .method=HTTP_GET, .handler=h_captive_redirect},
        {.uri="/library/test/success.html", .method=HTTP_GET, .handler=h_captive_redirect},
        {.uri="/ncsi.txt", .method=HTTP_GET, .handler=h_captive_redirect},
        {.uri="/connecttest.txt", .method=HTTP_GET, .handler=h_captive_redirect},
        {.uri="/canonical.html", .method=HTTP_GET, .handler=h_captive_redirect},
    };
    for (size_t i = 0; i < sizeof(routes)/sizeof(routes[0]); i++)
        httpd_register_uri_handler(s_server, &routes[i]);
    ESP_LOGI(TAG, "HTTP http://%s/", wifi_net_get_ip());
    return ESP_OK;
}
