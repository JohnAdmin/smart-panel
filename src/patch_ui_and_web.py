import os

ui_file = 'ui/ui_settings.cpp'
with open(ui_file, 'r', encoding='utf-8') as f:
    ui_code = f.read()

# Replace the layout of ui_settings.cpp
tgt_ui = """  lv_obj_t *lbl_info = lv_label_create(set_container);
  lv_label_set_text(lbl_info, "Scan to configure via Web Portal:");
  lv_obj_set_style_text_color(lbl_info, CLR_TEXT_TITLE, 0);
  lv_obj_align(lbl_info, LV_ALIGN_TOP_MID, 0, 10);

  if (isWifiConnected) {
    String ipStr = WiFi.localIP().toString();
    String url = "http://" + ipStr;

    // Create QR Code (size 130x130 to prevent overlap with brightness slider)
    lv_obj_t *qr = lv_qrcode_create(set_container, 130, lv_color_black(),
                                    lv_color_white());
    lv_qrcode_update(qr, url.c_str(), url.length());
    lv_obj_align(qr, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t *lbl_ip = lv_label_create(set_container);
    lv_label_set_text(lbl_ip, url.c_str());
    lv_obj_set_style_text_color(lbl_ip, CLR_PRIMARY, 0);
    lv_obj_align(lbl_ip, LV_ALIGN_TOP_MID, 0, 180);
  } else {
    lv_obj_t *lbl_err = lv_label_create(set_container);
    lv_label_set_text(lbl_err, "Wi-Fi disconnected!\\nCannot start Web Portal.");
    lv_obj_set_style_text_align(lbl_err, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_err, lv_color_hex(0xFF0000), 0);
    lv_obj_align(lbl_err, LV_ALIGN_TOP_MID, 0, 80);

    lv_obj_t *btn_wifi = lv_btn_create(set_container);
    lv_obj_set_size(btn_wifi, 230, 45);
    lv_obj_align(btn_wifi, LV_ALIGN_TOP_MID, 0, 140);
    lv_obj_set_style_bg_color(btn_wifi, lv_color_make(0x1e, 0x3a, 0x8a), 0); // Professional deep blue
    lv_obj_set_style_bg_opa(btn_wifi, LV_OPA_80, 0);
    lv_obj_set_style_border_color(btn_wifi, lv_color_make(0x3b, 0x82, 0xf6), 0);
    lv_obj_set_style_border_width(btn_wifi, 1, 0);
    lv_obj_set_style_radius(btn_wifi, 16, 0);
    lv_obj_t *lbl_wifi = lv_label_create(btn_wifi);
    lv_label_set_text(lbl_wifi, LV_SYMBOL_WIFI "  Manual Wi-Fi Setup");
    lv_obj_center(lbl_wifi);
    lv_obj_add_event_cb(btn_wifi, btn_wifi_config_cb, LV_EVENT_ALL, NULL);
  }

  // --- Brightness Slider ---
  lv_obj_t *br_label = lv_label_create(set_container);
  lv_label_set_text(br_label, "Brightness");
  lv_obj_set_style_text_color(br_label, CLR_TEXT_DIM, 0);
  lv_obj_align(br_label, LV_ALIGN_TOP_MID, -150, 205);

  lv_obj_t *slider = lv_slider_create(set_container);
  lv_obj_set_width(slider, 130);
  lv_obj_set_style_bg_color(slider, lv_color_hex(0x3F3F46), LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, CLR_PRIMARY, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
  lv_slider_set_range(slider, 10, 255);
  lv_slider_set_value(slider, displayBrightness, LV_ANIM_OFF);
  lv_obj_align(slider, LV_ALIGN_TOP_MID, -150, 232);
  lv_obj_add_event_cb(slider, brightness_slider_event_cb, LV_EVENT_ALL, NULL);

  // --- Screensaver Style Dropdown ---
  lv_obj_t *ss_label = lv_label_create(set_container);
  lv_label_set_text(ss_label, "Screensaver");
  lv_obj_set_style_text_color(ss_label, CLR_TEXT_DIM, 0);
  lv_obj_align(ss_label, LV_ALIGN_TOP_MID, 0, 205);

  lv_obj_t *dd_ss = lv_dropdown_create(set_container);
  lv_dropdown_set_options(dd_ss, "Flip Clock\\nMinimal\\nScreen Off");
  lv_dropdown_set_selected(dd_ss, screensaverStyle);
  lv_obj_set_width(dd_ss, 130);
  lv_obj_align(dd_ss, LV_ALIGN_TOP_MID, 0, 228);
  lv_obj_set_style_bg_color(dd_ss, lv_color_hex(0x27272A), 0);
  lv_obj_set_style_text_color(dd_ss, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_color(dd_ss, lv_color_hex(0x3F3F46), 0);
  lv_obj_set_style_border_width(dd_ss, 1, 0);
  lv_obj_set_style_radius(dd_ss, 12, 0);
  lv_obj_add_event_cb(
      dd_ss,
      [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
          lv_obj_t *dd = lv_event_get_target(e);
          screensaverStyle = lv_dropdown_get_selected(dd);
          preferences.begin("smartpanel", false);
          preferences.putInt("ss_style", screensaverStyle);
          preferences.end();
        }
      },
      LV_EVENT_ALL, NULL);

  // --- Timeout Dropdown ---
  lv_obj_t *to_label = lv_label_create(set_container);
  lv_label_set_text(to_label, "Timeout");
  lv_obj_set_style_text_color(to_label, CLR_TEXT_DIM, 0);
  lv_obj_align(to_label, LV_ALIGN_TOP_MID, 150, 205);

  lv_obj_t *dd_to = lv_dropdown_create(set_container);
  lv_dropdown_set_options(dd_to, "1 Min\\n2 Min\\n5 Min\\nNever");
  if (screensaverTimeoutMs == 60000)
    lv_dropdown_set_selected(dd_to, 0);
  else if (screensaverTimeoutMs == 120000)
    lv_dropdown_set_selected(dd_to, 1);
  else if (screensaverTimeoutMs == 300000)
    lv_dropdown_set_selected(dd_to, 2);
  else
    lv_dropdown_set_selected(dd_to, 3);
  lv_obj_set_width(dd_to, 110);
  lv_obj_align(dd_to, LV_ALIGN_TOP_MID, 150, 228);
  lv_obj_set_style_bg_color(dd_to, lv_color_hex(0x27272A), 0);
  lv_obj_set_style_text_color(dd_to, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_color(dd_to, lv_color_hex(0x3F3F46), 0);
  lv_obj_set_style_border_width(dd_to, 1, 0);
  lv_obj_set_style_radius(dd_to, 12, 0);
  lv_obj_add_event_cb(
      dd_to,
      [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
          lv_obj_t *dd = lv_event_get_target(e);
          int sel = lv_dropdown_get_selected(dd);
          if (sel == 0)
            screensaverTimeoutMs = 60000;
          else if (sel == 1)
            screensaverTimeoutMs = 120000;
          else if (sel == 2)
            screensaverTimeoutMs = 300000;
          else
            screensaverTimeoutMs = 0; // 0 will disable screensaver condition
          preferences.begin("smartpanel", false);
          preferences.end();
        }
      },
      LV_EVENT_ALL, NULL);

  // --- Reset Web Auth Button ---
  lv_obj_t *btn_reset = lv_btn_create(set_container);
  lv_obj_set_size(btn_reset, 190, 40);
  lv_obj_align(btn_reset, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_set_style_bg_color(btn_reset, lv_color_hex(0x7F1D1D), 0);
  lv_obj_set_style_bg_opa(btn_reset, LV_OPA_80, 0);
  lv_obj_set_style_border_color(btn_reset, lv_color_hex(0xEF4444), 0);
  lv_obj_set_style_border_width(btn_reset, 1, 0);
  lv_obj_set_style_radius(btn_reset, 16, 0);
  lv_obj_t *lbl_reset = lv_label_create(btn_reset);
  lv_label_set_text(lbl_reset, LV_SYMBOL_WARNING " Reset Web Auth");
  lv_obj_center(lbl_reset);
  lv_obj_add_event_cb(
      btn_reset,
      [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
          applySettings(wifi_ssid.c_str(), wifi_pass.c_str(),
                        mqtt_server_ip.c_str(), mqtt_username.c_str(),
                        mqtt_password.c_str(), weatherCity, panelTitle,
                        themeDark, useLargeTiles, displayBrightness, "admin",
                        "admin");
        }
      },
      LV_EVENT_ALL, NULL);"""

rep_ui = """  // Left Column Layout (-120 X offset) for Web Config
  lv_obj_t *lbl_info = lv_label_create(set_container);
  lv_label_set_text(lbl_info, "Web Portal Setup:");
  lv_obj_set_style_text_color(lbl_info, CLR_TEXT_TITLE, 0);
  lv_obj_align(lbl_info, LV_ALIGN_TOP_MID, -120, 10);

  if (isWifiConnected) {
    String ipStr = WiFi.localIP().toString();
    String url = "http://" + ipStr;

    lv_obj_t *qr = lv_qrcode_create(set_container, 120, lv_color_black(),
                                    lv_color_white());
    lv_qrcode_update(qr, url.c_str(), url.length());
    lv_obj_align(qr, LV_ALIGN_TOP_MID, -120, 35);

    lv_obj_t *lbl_ip = lv_label_create(set_container);
    lv_label_set_text(lbl_ip, url.c_str());
    lv_obj_set_style_text_color(lbl_ip, CLR_PRIMARY, 0);
    lv_obj_align(lbl_ip, LV_ALIGN_TOP_MID, -120, 165);
    
    // Reset Web Auth cleanly positioned below IP Address
    lv_obj_t *btn_reset = lv_btn_create(set_container);
    lv_obj_set_size(btn_reset, 170, 38);
    lv_obj_align(btn_reset, LV_ALIGN_TOP_MID, -120, 195);
    lv_obj_set_style_bg_color(btn_reset, lv_color_hex(0x7F1D1D), 0);
    lv_obj_set_style_bg_opa(btn_reset, LV_OPA_80, 0);
    lv_obj_set_style_border_color(btn_reset, lv_color_hex(0xEF4444), 0);
    lv_obj_set_style_border_width(btn_reset, 1, 0);
    lv_obj_set_style_radius(btn_reset, 12, 0);
    lv_obj_t *lbl_reset = lv_label_create(btn_reset);
    lv_label_set_text(lbl_reset, LV_SYMBOL_WARNING " Reset Password");
    lv_obj_center(lbl_reset);
    lv_obj_add_event_cb(
        btn_reset,
        [](lv_event_t *e) {
          if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            applySettings(wifi_ssid.c_str(), wifi_pass.c_str(),
                          mqtt_server_ip.c_str(), mqtt_username.c_str(),
                          mqtt_password.c_str(), weatherCity, panelTitle,
                          themeDark, useLargeTiles, displayBrightness, use24HourFormat, "admin",
                          "admin");
          }
        },
        LV_EVENT_ALL, NULL);
  } else {
    lv_obj_t *lbl_err = lv_label_create(set_container);
    lv_label_set_text(lbl_err, "Disconnected!\\nNo Web Portal.");
    lv_obj_set_style_text_align(lbl_err, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_err, lv_color_hex(0xFF0000), 0);
    lv_obj_align(lbl_err, LV_ALIGN_TOP_MID, -120, 60);

    lv_obj_t *btn_wifi = lv_btn_create(set_container);
    lv_obj_set_size(btn_wifi, 190, 45);
    lv_obj_align(btn_wifi, LV_ALIGN_TOP_MID, -120, 120);
    lv_obj_set_style_bg_color(btn_wifi, lv_color_make(0x1e, 0x3a, 0x8a), 0);
    lv_obj_set_style_bg_opa(btn_wifi, LV_OPA_80, 0);
    lv_obj_set_style_border_color(btn_wifi, lv_color_make(0x3b, 0x82, 0xf6), 0);
    lv_obj_set_style_border_width(btn_wifi, 1, 0);
    lv_obj_set_style_radius(btn_wifi, 16, 0);
    lv_obj_t *lbl_wifi = lv_label_create(btn_wifi);
    lv_label_set_text(lbl_wifi, LV_SYMBOL_WIFI " Setup Wi-Fi");
    lv_obj_center(lbl_wifi);
    lv_obj_add_event_cb(btn_wifi, btn_wifi_config_cb, LV_EVENT_ALL, NULL);
  }

  // Right Column Layout (+120 X offset) for Panel Settings
  // --- Brightness Slider ---
  lv_obj_t *br_label = lv_label_create(set_container);
  lv_label_set_text(br_label, "Brightness");
  lv_obj_set_style_text_color(br_label, CLR_TEXT_DIM, 0);
  lv_obj_align(br_label, LV_ALIGN_TOP_MID, 120, 15);

  lv_obj_t *slider = lv_slider_create(set_container);
  lv_obj_set_width(slider, 160);
  lv_obj_set_style_bg_color(slider, lv_color_hex(0x3F3F46), LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, CLR_PRIMARY, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
  lv_slider_set_range(slider, 10, 255);
  lv_slider_set_value(slider, displayBrightness, LV_ANIM_OFF);
  lv_obj_align(slider, LV_ALIGN_TOP_MID, 120, 40);
  lv_obj_add_event_cb(slider, brightness_slider_event_cb, LV_EVENT_ALL, NULL);

  // --- Screensaver Style Dropdown ---
  lv_obj_t *ss_label = lv_label_create(set_container);
  lv_label_set_text(ss_label, "Screensaver");
  lv_obj_set_style_text_color(ss_label, CLR_TEXT_DIM, 0);
  lv_obj_align(ss_label, LV_ALIGN_TOP_MID, 120, 85);

  lv_obj_t *dd_ss = lv_dropdown_create(set_container);
  lv_dropdown_set_options(dd_ss, "Flip Clock\\nMinimal\\nScreen Off");
  lv_dropdown_set_selected(dd_ss, screensaverStyle);
  lv_obj_set_width(dd_ss, 160);
  lv_obj_align(dd_ss, LV_ALIGN_TOP_MID, 120, 110);
  lv_obj_set_style_bg_color(dd_ss, lv_color_hex(0x27272A), 0);
  lv_obj_set_style_text_color(dd_ss, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_color(dd_ss, lv_color_hex(0x3F3F46), 0);
  lv_obj_set_style_border_width(dd_ss, 1, 0);
  lv_obj_set_style_radius(dd_ss, 12, 0);
  lv_obj_add_event_cb(
      dd_ss,
      [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
          lv_obj_t *dd = lv_event_get_target(e);
          screensaverStyle = lv_dropdown_get_selected(dd);
          preferences.begin("smartpanel", false);
          preferences.putInt("ss_style", screensaverStyle);
          preferences.end();
        }
      },
      LV_EVENT_ALL, NULL);

  // --- Timeout Dropdown ---
  lv_obj_t *to_label = lv_label_create(set_container);
  lv_label_set_text(to_label, "Timeout");
  lv_obj_set_style_text_color(to_label, CLR_TEXT_DIM, 0);
  lv_obj_align(to_label, LV_ALIGN_TOP_MID, 120, 155);

  lv_obj_t *dd_to = lv_dropdown_create(set_container);
  lv_dropdown_set_options(dd_to, "1 Min\\n2 Min\\n5 Min\\nNever");
  if (screensaverTimeoutMs == 60000)
    lv_dropdown_set_selected(dd_to, 0);
  else if (screensaverTimeoutMs == 120000)
    lv_dropdown_set_selected(dd_to, 1);
  else if (screensaverTimeoutMs == 300000)
    lv_dropdown_set_selected(dd_to, 2);
  else
    lv_dropdown_set_selected(dd_to, 3);
  lv_obj_set_width(dd_to, 160);
  lv_obj_align(dd_to, LV_ALIGN_TOP_MID, 120, 180);
  lv_obj_set_style_bg_color(dd_to, lv_color_hex(0x27272A), 0);
  lv_obj_set_style_text_color(dd_to, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_color(dd_to, lv_color_hex(0x3F3F46), 0);
  lv_obj_set_style_border_width(dd_to, 1, 0);
  lv_obj_set_style_radius(dd_to, 12, 0);
  lv_obj_add_event_cb(
      dd_to,
      [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
          lv_obj_t *dd = lv_event_get_target(e);
          int sel = lv_dropdown_get_selected(dd);
          if (sel == 0)
            screensaverTimeoutMs = 60000;
          else if (sel == 1)
            screensaverTimeoutMs = 120000;
          else if (sel == 2)
            screensaverTimeoutMs = 300000;
          else
            screensaverTimeoutMs = 0; // 0 will disable screensaver condition
          preferences.begin("smartpanel", false);
          preferences.end();
        }
      },
      LV_EVENT_ALL, NULL);

  // --- Time Format Dropdown ---
  lv_obj_t *tf_label = lv_label_create(set_container);
  lv_label_set_text(tf_label, "Time Format");
  lv_obj_set_style_text_color(tf_label, CLR_TEXT_DIM, 0);
  lv_obj_align(tf_label, LV_ALIGN_TOP_MID, 120, 225);

  lv_obj_t *dd_tf = lv_dropdown_create(set_container);
  lv_dropdown_set_options(dd_tf, "12 Hour\\n24 Hour");
  lv_dropdown_set_selected(dd_tf, use24HourFormat ? 1 : 0);
  lv_obj_set_width(dd_tf, 160);
  lv_obj_align(dd_tf, LV_ALIGN_TOP_MID, 120, 250);
  lv_obj_set_style_bg_color(dd_tf, lv_color_hex(0x27272A), 0);
  lv_obj_set_style_text_color(dd_tf, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_color(dd_tf, lv_color_hex(0x3F3F46), 0);
  lv_obj_set_style_border_width(dd_tf, 1, 0);
  lv_obj_set_style_radius(dd_tf, 12, 0);
  lv_obj_add_event_cb(
      dd_tf,
      [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
          lv_obj_t *dd = lv_event_get_target(e);
          use24HourFormat = (lv_dropdown_get_selected(dd) == 1);
          preferences.begin("smartpanel", false);
          preferences.putBool("time_24h", use24HourFormat);
          preferences.end();
        }
      },
      LV_EVENT_ALL, NULL);"""

if tgt_ui in ui_code:
    ui_code = ui_code.replace(tgt_ui, rep_ui)
    with open(ui_file, 'w', encoding='utf-8') as f:
        f.write(ui_code)
    print("ui_settings.cpp successfully split to 2-columns!")
else:
    print("Failed to replace ui_settings.cpp block.")

# web_server.cpp part
web_file = 'web_server.cpp'
with open(web_file, 'r', encoding='utf-8') as f:
    web_code = f.read()

# Add SVG toggle to pwd fields
def wrap_pwd(field_id, placeholder):
    old = f'<input type="password" id="{field_id}" class="w-full bg-zinc-900/50 border border-zinc-700 rounded-2xl px-4 py-3 text-zinc-200 focus:outline-none focus:ring-2 focus:ring-primary/50 focus:border-primary transition-all placeholder-zinc-600" placeholder="{placeholder}">'
    new = f'''<div class="relative">
                            <input type="password" id="{field_id}" class="w-full bg-zinc-900/50 border border-zinc-700 rounded-2xl px-4 py-3 pr-12 text-zinc-200 focus:outline-none focus:ring-2 focus:ring-primary/50 focus:border-primary transition-all placeholder-zinc-600" placeholder="{placeholder}">
                            <button type="button" onclick="togglePwd('{field_id}', this)" class="absolute inset-y-0 right-0 pr-4 flex items-center text-zinc-400 hover:text-primary transition-colors focus:outline-none">
                                <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5 eye-icon" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15 12a3 3 0 11-6 0 3 3 0 016 0z" />
                                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M2.458 12C3.732 7.943 7.523 5 12 5c4.478 0 8.268 2.943 9.542 7-1.274 4.057-5.064 7-9.542 7-4.477 0-8.268-2.943-9.542-7z" />
                                </svg>
                            </button>
                        </div>'''
    return old, new

p1o, p1n = wrap_pwd('wifi_pass', '••••••••')
p2o, p2n = wrap_pwd('mqtt_pwd', '')
# web pass has a placeholder "admin" natively
p3o, p3n = wrap_pwd('web_pass', 'admin')

js_func = """<script>
    function togglePwd(id, btn) {
        let input = document.getElementById(id);
        let svg = btn.querySelector('.eye-icon');
        if(input.type === "password") {
            input.type = "text";
            svg.innerHTML = '<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M13.875 18.825A10.05 10.05 0 0112 19c-4.478 0-8.268-2.943-9.543-7a9.97 9.97 0 011.563-3.029m5.858.908a3 3 0 114.243 4.243M9.878 9.878l4.242 4.242M9.88 9.88l-3.29-3.29m7.532 7.532l3.29 3.29M3 3l3.59 3.59m0 0A9.953 9.953 0 0112 5c4.478 0 8.268 2.943 9.543 7a10.025 10.025 0 01-4.132 5.411m0 0L21 21" />';
        } else {
            input.type = "password";
            svg.innerHTML = '<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15 12a3 3 0 11-6 0 3 3 0 016 0z" /><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M2.458 12C3.732 7.943 7.523 5 12 5c4.478 0 8.268 2.943 9.542 7-1.274 4.057-5.064 7-9.542 7-4.477 0-8.268-2.943-9.542-7z" />';
        }
    }"""

if p1o in web_code and p2o in web_code and p3o in web_code:
    web_code = web_code.replace(p1o, p1n)
    web_code = web_code.replace(p2o, p2n)
    web_code = web_code.replace(p3o, p3n)
    web_code = web_code.replace('<script>', js_func, 1)
    with open(web_file, 'w', encoding='utf-8') as f:
        f.write(web_code)
    print("web_server.cpp successfully added eye icons!")
else:
    print("Failed to replace web_server.cpp password fields.")
    if p1o not in web_code: print("Missing wifi_pass block")
    if p2o not in web_code: print("Missing mqtt_pwd block")
    if p3o not in web_code: print("Missing web_pass block")
