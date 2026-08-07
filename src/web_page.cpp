// web_page.cpp
// Single-page web portal served at GET / by web_server.cpp.
// Extracted verbatim from web_server.cpp -- markup is unchanged.

#include "web_page.h"
#include <Arduino.h>

// ========================================================
//  FRONTEND HTML (Single Page App)
// ========================================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en" class="dark">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Hero Home Configuration</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Fira+Code:wght@400;500;600;700&family=Fira+Sans:wght@300;400;500;600;700&display=swap" rel="stylesheet">
    <script src="https://cdn.tailwindcss.com"></script>
    <script>
        tailwind.config = {
            darkMode: 'class',
            theme: {
                extend: {
                    fontFamily: {
                        sans: ['Fira Sans', 'ui-sans-serif', 'system-ui', 'sans-serif'],
                        mono: ['Fira Code', 'ui-monospace', 'SFMono-Regular', 'Menlo', 'Monaco', 'Consolas', 'monospace'],
                    },
                    colors: {
                        primary: '#3B82F6',
                        primaryHover: '#1D4ED8',
                        bgDark: '#0F1419',
                        cardDark: '#1A202C',
                        borderDark: '#2D3748',
                        up: '#22C55E',
                        down: '#EF4444',
                        neutral: '#8B92A9'
                    },
                    spacing: {
                        'xs': '0.25rem',
                        'sm': '0.5rem',
                        'md': '1rem',
                        'lg': '1.5rem',
                        'xl': '2rem'
                    },
                    borderRadius: {
                        'none': '0',
                        'sm': '0.375rem',
                        'md': '0.5rem',
                        'lg': '0.75rem',
                        'xl': '1rem',
                        '2xl': '1.5rem',
                        '3xl': '2rem',
                        'full': '9999px'
                    },
                    boxShadow: {
                        'xs': '0 1px 2px 0 rgba(0, 0, 0, 0.05)',
                        'sm': '0 1px 3px 0 rgba(0, 0, 0, 0.1)',
                        'md': '0 4px 6px -1px rgba(0, 0, 0, 0.1)',
                        'lg': '0 10px 15px -3px rgba(0, 0, 0, 0.1)',
                        'xl': '0 20px 25px -5px rgba(0, 0, 0, 0.1)',
                        '2xl': '0 25px 50px -12px rgba(0, 0, 0, 0.25)'
                    }
                }
            }
        }
    </script>
    <style>
        body { background-color: #0F1419; color: #F0F4F8; }
        ::-webkit-scrollbar { width: 8px; }
        ::-webkit-scrollbar-track { background: #0F1419; }
        ::-webkit-scrollbar-thumb { background: #2D3748; border-radius: 4px; }
        ::-webkit-scrollbar-thumb:hover { background: #4A5568; }
        .glass-header {
            background: rgba(15, 20, 25, 0.92);
            backdrop-filter: blur(16px);
            -webkit-backdrop-filter: blur(16px);
        }
        .glass-panel {
            background: rgba(26, 32, 44, 0.78);
            backdrop-filter: blur(16px);
            -webkit-backdrop-filter: blur(16px);
        }

        /* Keep focus visible and consistent for keyboard users */
        button:focus-visible,
        [role="tab"]:focus-visible,
        input:focus-visible,
        select:focus-visible,
        textarea:focus-visible,
        a:focus-visible {
            outline: 2px solid #22c55e;
            outline-offset: 2px;
        }

        /* Enforce clear affordance for all interactive elements */
        button,
        [onclick],
        [role="tab"] {
            cursor: pointer;
        }

        /* Input / select hover affordance */
        input:not(:focus):hover,
        select:not(:focus):hover,
        textarea:not(:focus):hover {
            border-color: #4b5563;
        }

        /* Custom select chevron (replaces browser default with consistent icon) */
        select {
            background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 20 20' fill='%236b7280'%3E%3Cpath fill-rule='evenodd' d='M5.293 7.293a1 1 0 011.414 0L10 10.586l3.293-3.293a1 1 0 111.414 1.414l-4 4a1 1 0 01-1.414 0l-4-4a1 1 0 010-1.414z' clip-rule='evenodd'/%3E%3C/svg%3E");
            background-repeat: no-repeat;
            background-position: right 0.75rem center;
            background-size: 1rem;
            padding-right: 2.5rem;
        }

        /* Active tab — blue accent (works alongside JS-added classes) */
        .tab-btn.active {
            background-color: rgba(59, 130, 246, 0.12);
            color: #3B82F6;
            border-color: rgba(59, 130, 246, 0.4);
        }

        /* Section h3 headings — left accent bar for visual hierarchy */
        .glass-panel h3.uppercase {
            border-left: 3px solid rgba(59, 130, 246, 0.6);
            padding-left: 0.75rem;
        }
        /* Active lang button */
        .lang-btn[aria-pressed="true"] {
            background-color: rgba(59, 130, 246, 0.15);
            color: #3B82F6;
            border: 1px solid rgba(59, 130, 246, 0.4);
        }
    </style>
</head>
<body class="bg-bgDark text-slate-100 antialiased selection:bg-primary selection:text-white min-h-screen pb-24 font-sans max-w-[100vw] overflow-x-hidden">

    <!-- Header -->
    <header class="glass-header border-b border-borderDark sticky top-0 z-50 shadow-lg">
        <div class="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-4 flex flex-col md:flex-row gap-4 justify-between items-center">
            <div class="flex items-center gap-3">
                <div class="bg-primary/15 p-2 rounded-lg border border-primary/30 text-primary">
                    <svg xmlns="http://www.w3.org/2000/svg" class="h-6 w-6" viewBox="0 0 20 20" fill="currentColor">
                        <path d="M10.707 2.293a1 1 0 00-1.414 0l-7 7a1 1 0 001.414 1.414L4 10.414V17a1 1 0 001 1h2a1 1 0 001-1v-2a1 1 0 011-1h2a1 1 0 011 1v2a1 1 0 001 1h2a1 1 0 001-1v-6.586l.293.293a1 1 0 001.414-1.414l-7-7z" />
                    </svg>
                </div>
                <h1 class="text-2xl font-bold text-slate-100">Smart Control Panel</h1>
            </div>
            <!-- Language switcher -->
            <div class="flex items-center gap-1 bg-slate-900/60 rounded-lg border border-slate-700/60 p-1" role="group" aria-label="Language">
                <button id="lang-en" type="button" onclick="setLang('en')" class="lang-btn px-3 py-1.5 rounded-lg text-xs font-semibold transition-all duration-200 text-slate-400 hover:text-slate-200" aria-pressed="false">EN</button>
                <button id="lang-th" type="button" onclick="setLang('th')" class="lang-btn px-3 py-1.5 rounded-lg text-xs font-semibold transition-all duration-200 text-slate-400 hover:text-slate-200" aria-pressed="false">ภาษาไทย</button>
            </div>
            <button onclick="saveConfiguration()" id="saveBtn" class="bg-gradient-to-r from-primary to-primaryHover hover:from-primaryHover hover:to-blue-700 text-white font-semibold py-2.5 px-6 rounded-lg shadow-lg shadow-primary/30 hover:shadow-primary/50 transition-all duration-300 transform hover:-translate-y-1 flex items-center gap-2 w-full md:w-auto justify-center ring-1 ring-white/10">
                <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5" viewBox="0 0 20 20" fill="currentColor">
                  <path fill-rule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clip-rule="evenodd" />
                </svg>
                Save & Restart
            </button>
        </div>
    </header>

    <main class="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8 space-y-6 animate-fade-in-up">
        
        <!-- Tab Navigation -->
        <div id="tablist-main" role="tablist" aria-label="Configuration Sections" class="flex flex-wrap gap-2 bg-slate-800/50 p-1.5 rounded-lg border border-slate-700/60 shadow-sm w-full">
            <button id="btn-tab-network" role="tab" aria-controls="tab-network" aria-selected="true" tabindex="0" type="button" onclick="switchTab('tab-network', this)" class="tab-btn active whitespace-nowrap px-5 py-2 rounded-lg text-sm font-semibold transition-all duration-200 text-primary bg-primary/12 shadow-sm shadow-primary/20 border border-primary/35">
                Network Setup
            </button>
            <button id="btn-tab-wallpaper" role="tab" aria-controls="tab-wallpaper" aria-selected="false" tabindex="-1" type="button" onclick="switchTab('tab-wallpaper', this)" class="tab-btn whitespace-nowrap px-5 py-2 rounded-lg text-sm font-semibold text-slate-400 hover:text-slate-200 transition-all duration-200 border border-transparent hover:bg-slate-700/40">
                Wallpaper
            </button>
            <button id="btn-tab-devices" role="tab" aria-controls="tab-devices" aria-selected="false" tabindex="-1" type="button" onclick="switchTab('tab-devices', this)" class="tab-btn whitespace-nowrap px-5 py-2 rounded-lg text-sm font-semibold text-slate-400 hover:text-slate-200 transition-all duration-200 border border-transparent hover:bg-slate-700/40">
                Device Management
            </button>
            <button id="btn-tab-scenes" role="tab" aria-controls="tab-scenes" aria-selected="false" tabindex="-1" type="button" onclick="switchTab('tab-scenes', this)" class="tab-btn whitespace-nowrap px-5 py-2 rounded-lg text-sm font-semibold text-slate-400 hover:text-slate-200 transition-all duration-200 border border-transparent hover:bg-slate-700/40">
                Scenes
            </button>
            <button id="btn-tab-schedules" role="tab" aria-controls="tab-schedules" aria-selected="false" tabindex="-1" type="button" onclick="switchTab('tab-schedules', this)" class="tab-btn whitespace-nowrap px-5 py-2 rounded-lg text-sm font-semibold text-slate-400 hover:text-slate-200 transition-all duration-200 border border-transparent hover:bg-slate-700/40">
                Schedules
            </button>
            <button id="btn-tab-stocks" role="tab" aria-controls="tab-stocks" aria-selected="false" tabindex="-1" type="button" onclick="switchTab('tab-stocks', this)" class="tab-btn whitespace-nowrap px-5 py-2 rounded-lg text-sm font-semibold text-slate-400 hover:text-slate-200 transition-all duration-200 border border-transparent hover:bg-slate-700/40">
                Stock
            </button>
            <button id="btn-tab-system" role="tab" aria-controls="tab-system" aria-selected="false" tabindex="-1" type="button" onclick="switchTab('tab-system', this)" class="tab-btn whitespace-nowrap px-5 py-2 rounded-lg text-sm font-semibold text-slate-400 hover:text-slate-200 transition-all duration-200 border border-transparent hover:bg-slate-700/40">
                System & Updates
            </button>
        </div>

        <!-- Network Settings Tab -->
        <section id="tab-network" role="tabpanel" aria-labelledby="btn-tab-network" tabindex="0" class="tab-pane block glass-panel rounded-2xl shadow-xl border border-borderDark overflow-hidden transition-all duration-500 hover:border-slate-600 hover:shadow-lg">
            <div class="border-b border-slate-700/50 px-6 py-4 bg-slate-800/20 flex items-center gap-3">
                <div class="bg-primary/15 text-primary p-2 rounded-lg">
                    <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                      <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M8.111 16.404a5.5 5.5 0 017.778 0M12 20h.01m-7.08-7.071c3.904-3.905 10.236-3.906 14.142 0M1.394 9.393c5.857-5.857 15.355-5.857 21.213 0" />
                    </svg>
                </div>
                <h2 class="text-lg font-semibold text-slate-100">Network & MQTT Configuration</h2>
            </div>
            <div class="p-6 grid grid-cols-1 md:grid-cols-2 gap-x-10 gap-y-8">
                <!-- WiFi -->
                <div class="space-y-5 md:border-r md:border-slate-700/40 md:pr-6">
                    <h3 class="text-sm font-semibold text-slate-400 uppercase tracking-wider mb-2">Wi-Fi Credentials</h3>
                    <div>
                        <label class="block text-sm font-medium text-slate-300 mb-1.5">SSID</label>
                        <input type="text" id="wifi_ssid" class="w-full bg-slate-900/50 border border-slate-700 rounded-lg px-4 py-3 text-slate-100 focus:outline-none focus:ring-2 focus:ring-primary/50 focus:border-primary transition-all placeholder-slate-500" placeholder="Your Network Name">
                    </div>
                    <div>
                        <label class="block text-sm font-medium text-slate-300 mb-1.5">Password</label>
                        <div class="relative">
                            <input type="password" id="wifi_pass" class="w-full bg-slate-900/50 border border-slate-700 rounded-lg px-4 py-3 pr-12 text-slate-100 focus:outline-none focus:ring-2 focus:ring-primary/50 focus:border-primary transition-all placeholder-slate-500" placeholder="Password">
                            <button type="button" onclick="togglePwd('wifi_pass', this)" class="absolute inset-y-0 right-0 pr-4 flex items-center text-slate-500 hover:text-primary transition-colors focus:outline-none">
                                <svg class="h-5 w-5 eye-icon" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15 12a3 3 0 11-6 0 3 3 0 016 0z" />
                                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M2.458 12C3.732 7.943 7.523 5 12 5c4.478 0 8.268 2.943 9.542 7-1.274 4.057-5.064 7-9.542 7-4.477 0-8.268-2.943-9.542-7z" />
                                </svg>
                            </button>
                        </div>
                    </div>
                </div>
                <!-- MQTT & Weather -->
                <div class="space-y-5">
                    <h3 class="text-sm font-semibold text-slate-400 uppercase tracking-wider mb-2">Broker & Settings</h3>
                    <div>
                        <label class="block text-sm font-medium text-slate-300 mb-1.5">MQTT Server IP</label>
                        <div class="flex relative">
                            <span class="inline-flex items-center px-3 rounded-l-lg border border-r-0 border-slate-700 bg-slate-800 text-slate-500 text-sm">
                                tcp://
                            </span>
                            <input type="text" id="mqtt_srv" class="flex-1 w-full bg-slate-900/50 border border-slate-700 px-4 py-2.5 text-slate-100 focus:outline-none focus:ring-2 focus:ring-primary/50 focus:border-primary transition-all placeholder-slate-500 focus:z-10" placeholder="192.168.1.xxx">
                            <span class="inline-flex items-center px-2 border border-l-0 border-r-0 border-slate-700 bg-slate-800 text-slate-500 text-sm">:</span>
                            <input type="number" id="mqtt_port" class="w-20 bg-slate-900/50 border border-slate-700 rounded-r-lg px-3 py-2.5 text-slate-100 focus:outline-none focus:ring-2 focus:ring-primary/50 focus:border-primary transition-all placeholder-slate-500 focus:z-10" placeholder="1883" min="1" max="65535">
                        </div>
                    </div>
                    <div class="grid grid-cols-1 sm:grid-cols-2 gap-4">
                        <div>
                            <label class="block text-sm font-medium text-slate-300 mb-1.5">Username</label>
                            <input type="text" id="mqtt_usr" class="w-full bg-slate-900/50 border border-slate-700 rounded-lg px-4 py-3 text-slate-100 focus:outline-none focus:ring-2 focus:ring-primary/50 focus:border-primary transition-all placeholder-slate-500">
                        </div>
                        <div>
                            <label class="block text-sm font-medium text-slate-300 mb-1.5">Password</label>
                            <div class="relative">
                                <input type="password" id="mqtt_pwd" class="w-full bg-slate-900/50 border border-slate-700 rounded-lg px-4 py-3 pr-12 text-slate-100 focus:outline-none focus:ring-2 focus:ring-primary/50 focus:border-primary transition-all placeholder-slate-500">
                                <button type="button" onclick="togglePwd('mqtt_pwd', this)" class="absolute inset-y-0 right-0 pr-4 flex items-center text-slate-500 hover:text-primary transition-colors focus:outline-none">
                                    <svg class="h-5 w-5 eye-icon" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15 12a3 3 0 11-6 0 3 3 0 016 0z" />
                                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M2.458 12C3.732 7.943 7.523 5 12 5c4.478 0 8.268 2.943 9.542 7-1.274 4.057-5.064 7-9.542 7-4.477 0-8.268-2.943-9.542-7z" />
                                    </svg>
                                </button>
                            </div>
                        </div>
                    </div>
                    <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
                        <div>
                            <label class="block text-sm font-medium text-slate-300 mb-1.5">Weather City <span class="text-[10px] text-slate-500 font-normal block">(Screensaver &amp; Air Quality)</span></label>
                            <div id="city-field" class="relative">
                                <input type="text" id="weather_city" autocomplete="off" oninput="onCityInput()" class="w-full bg-slate-900/50 border border-slate-700 rounded-lg px-4 py-3 text-slate-100 focus:outline-none focus:ring-2 focus:ring-primary/50 focus:border-primary transition-all placeholder-slate-500" placeholder="Start typing a city...">
                                <div id="city-results" class="hidden absolute z-20 mt-1 w-full bg-slate-800 border border-slate-600 rounded-lg shadow-xl overflow-hidden max-h-60 overflow-y-auto"></div>
                                <div class="grid grid-cols-2 gap-2 mt-2">
                                    <input type="number" step="any" id="weather_lat" oninput="onCoordInput()" class="bg-slate-900/50 border border-slate-700 rounded-lg px-3 py-2 text-sm text-slate-200 font-mono focus:outline-none focus:border-primary placeholder-slate-600" placeholder="Latitude">
                                    <input type="number" step="any" id="weather_lon" oninput="onCoordInput()" class="bg-slate-900/50 border border-slate-700 rounded-lg px-3 py-2 text-sm text-slate-200 font-mono focus:outline-none focus:border-primary placeholder-slate-600" placeholder="Longitude">
                                </div>
                                <div class="flex items-center justify-between mt-1 gap-2">
                                    <span id="city-pinned" class="text-[10px] text-slate-500"></span>
                                    <button type="button" onclick="clearCityCoords()" class="text-[10px] text-slate-500 hover:text-primary transition-colors whitespace-nowrap">Look up by name instead</button>
                                </div>
                                <p class="mt-1 text-[10px] text-slate-600">Pick a city above to fill these, or type coordinates for somewhere the search does not list &mdash; a Bangkok khet, a village, your own roof. Right-click a spot in Google Maps to read them off.</p>
                            </div>
                        </div>
                        <div>
                            <label class="block text-sm font-medium text-slate-300 mb-1.5">Panel Title</label>
                            <input type="text" id="panel_title" class="w-full bg-slate-900/50 border border-slate-700 rounded-lg px-4 py-3 text-slate-100 focus:outline-none focus:ring-2 focus:ring-primary/50 focus:border-primary transition-all placeholder-slate-500" placeholder="Smart Control Panel">
                        </div>
                    </div>
                    <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
                        <div>
                            <label class="block text-sm font-medium text-slate-300 mb-1.5">Time Format</label>
                            <select id="time_24h" class="w-full bg-slate-900/50 border border-slate-700 rounded-lg px-4 py-3 text-slate-100 focus:outline-none focus:ring-2 focus:ring-primary/50 focus:border-primary transition-all shadow-inner appearance-none cursor-pointer">
                                <option value="true">24-Hour</option>
                                <option value="false">12-Hour</option>
                            </select>
                        </div>
                        <div>
                            <label class="block text-sm font-medium text-slate-300 mb-1.5">Timezone</label>
                            <select id="gmt_offset" class="w-full bg-slate-900/50 border border-slate-700 rounded-lg px-4 py-3 text-slate-100 focus:outline-none focus:ring-2 focus:ring-primary/50 focus:border-primary transition-all shadow-inner appearance-none cursor-pointer">
                                <option value="-12">GMT-12</option><option value="-11">GMT-11</option>
                                <option value="-10">GMT-10</option><option value="-9">GMT-9</option>
                                <option value="-8">GMT-8</option><option value="-7">GMT-7</option>
                                <option value="-6">GMT-6</option><option value="-5">GMT-5</option>
                                <option value="-4">GMT-4</option><option value="-3">GMT-3</option>
                                <option value="-2">GMT-2</option><option value="-1">GMT-1</option>
                                <option value="0">GMT+0</option><option value="1">GMT+1</option>
                                <option value="2">GMT+2</option><option value="3">GMT+3</option>
                                <option value="4">GMT+4</option><option value="5">GMT+5</option>
                                <option value="6">GMT+6</option>
                                <option value="7" selected>GMT+7</option><option value="8">GMT+8</option>
                                <option value="9">GMT+9</option><option value="10">GMT+10</option>
                                <option value="11">GMT+11</option><option value="12">GMT+12</option>
                            </select>
                        </div>
                    </div>
                    
                    <div>
                        <label class="block text-sm font-medium text-slate-300 mb-1.5">Physical Screen Theme</label>
                        <select id="theme_dark" class="w-full bg-slate-900/50 border border-slate-700 rounded-2xl px-4 py-3 text-slate-200 focus:outline-none focus:ring-2 focus:ring-primary/50 focus:border-primary transition-all shadow-inner appearance-none cursor-pointer mb-4">
                            <option value="true">Dark Mode (Recommended)</option>
                            <option value="false">Light Mode</option>
                        </select>
                        <label class="block text-sm font-medium text-slate-300 mb-1.5">Grid Layout</label>
                        <select id="large_tiles" class="w-full bg-slate-900/50 border border-slate-700 rounded-lg px-4 py-3 text-slate-100 focus:outline-none focus:ring-2 focus:ring-primary/50 focus:border-primary transition-all shadow-inner appearance-none cursor-pointer mb-4">
                            <option value="false">6-Tile Layout (3 Columns)</option>
                            <option value="true">4-Tile Layout (Large Tiles)</option>
                        </select>
                    </div>

                    <!-- Web Portal Authentication -->
                    <div>
                        <h3 class="text-sm font-semibold text-slate-400 uppercase tracking-wider mb-2 mt-2">Web Portal Authentication</h3>
                        <div class="grid grid-cols-1 sm:grid-cols-2 gap-4">
                            <div>
                                <label class="block text-sm font-medium text-slate-300 mb-1.5">Web Username</label>
                                <input type="text" id="web_user" class="w-full bg-slate-900/50 border border-slate-700 rounded-lg px-4 py-3 text-slate-100 focus:outline-none focus:ring-2 focus:ring-primary/50 focus:border-primary transition-all placeholder-slate-500" placeholder="admin">
                            </div>
                            <div>
                                <label class="block text-sm font-medium text-slate-300 mb-1.5">Web Password</label>
                                <div class="relative">
                                    <input type="password" id="web_pass" class="w-full bg-slate-900/50 border border-slate-700 rounded-lg px-4 py-3 pr-12 text-slate-100 focus:outline-none focus:ring-2 focus:ring-primary/50 focus:border-primary transition-all placeholder-slate-500" placeholder="admin">
                                    <button type="button" onclick="togglePwd('web_pass', this)" class="absolute inset-y-0 right-0 pr-4 flex items-center text-slate-500 hover:text-primary transition-colors focus:outline-none">
                                        <svg class="h-5 w-5 eye-icon" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                                            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15 12a3 3 0 11-6 0 3 3 0 016 0z" />
                                            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M2.458 12C3.732 7.943 7.523 5 12 5c4.478 0 8.268 2.943 9.542 7-1.274 4.057-5.064 7-9.542 7-4.477 0-8.268-2.943-9.542-7z" />
                                        </svg>
                                    </button>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </section>

        <!-- Wallpaper & Screensaver Tab -->
        <section id="tab-wallpaper" role="tabpanel" aria-labelledby="btn-tab-wallpaper" tabindex="0" class="tab-pane hidden glass-panel rounded-2xl shadow-2xl border border-borderDark overflow-hidden transition-all duration-500 hover:border-slate-700 hover:shadow-primary/5">
            <div class="border-b border-borderDark px-6 py-4 bg-slate-800/30 flex items-center gap-3">
                <div class="bg-purple-500/20 text-purple-400 p-2 rounded-lg">
                    <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 16l4.586-4.586a2 2 0 012.828 0L16 16m-2-2l1.586-1.586a2 2 0 012.828 0L20 14m-6-6h.01M6 20h12a2 2 0 002-2V6a2 2 0 00-2-2H6a2 2 0 00-2 2v12a2 2 0 002 2z" />
                    </svg>
                </div>
                <h2 class="text-lg font-semibold text-slate-100">Wallpaper &amp; Screensaver</h2>
            </div>
            <div class="p-6 grid grid-cols-1 md:grid-cols-2 gap-8">
                <!-- Wallpaper Management -->
                <div class="space-y-4">
                    <h3 class="text-sm font-semibold text-slate-400 uppercase tracking-wider">Quick Select Presets</h3>
                    <div class="grid grid-cols-3 gap-3">
                        <div onclick="applyPreset(1)" class="cursor-pointer group relative overflow-hidden rounded-lg border-2 border-transparent hover:border-primary transition-all">
                            <img data-src="/default1.jpg" class="w-full aspect-[480/320] object-cover group-hover:scale-110 transition-transform duration-500 bg-slate-800">
                            <div class="absolute inset-x-0 bottom-0 bg-black/60 py-1 text-[10px] text-center text-slate-200">Deep Space</div>
                        </div>
                        <div onclick="applyPreset(2)" class="cursor-pointer group relative overflow-hidden rounded-lg border-2 border-transparent hover:border-primary transition-all">
                            <img data-src="/default2.jpg" class="w-full aspect-[480/320] object-cover group-hover:scale-110 transition-transform duration-500 bg-slate-800">
                            <div class="absolute inset-x-0 bottom-0 bg-black/60 py-1 text-[10px] text-center text-slate-200">Modern Dark</div>
                        </div>
                        <div onclick="applyPreset(3)" class="cursor-pointer group relative overflow-hidden rounded-lg border-2 border-transparent hover:border-primary transition-all">
                            <img data-src="/default3.jpg" class="w-full aspect-[480/320] object-cover group-hover:scale-110 transition-transform duration-500 bg-slate-800">
                            <div class="absolute inset-x-0 bottom-0 bg-black/60 py-1 text-[10px] text-center text-slate-200">Mystic Purple</div>
                        </div>
                    </div>

                    <div class="pt-4 border-t border-slate-800 space-y-4">
                        <h3 class="text-sm font-semibold text-slate-400 uppercase tracking-wider flex items-center gap-2">
                             Custom Background
                             <span id="wallpaper-status" class="text-[10px] text-slate-500 font-normal">Checking status...</span>
                        </h3>
                        <div class="flex flex-col gap-3">
                            <label class="block">
                                <span class="text-xs font-medium text-slate-400 mb-1.5 block">Upload JPEG (480x320)</span>
                                <input type="file" id="wallpaper-file" accept="image/jpeg,image/jpg" class="block w-full text-xs text-slate-500 file:mr-3 file:py-1.5 file:px-3 file:rounded-lg file:border-0 file:text-[11px] file:font-semibold file:bg-primary/20 file:text-primary hover:file:bg-primary/30 cursor-pointer">
                            </label>
                            <div class="flex gap-2">
                                <button onclick="uploadWallpaper()" class="bg-primary/80 hover:bg-primary text-white text-sm font-semibold py-2 px-4 rounded-lg transition-all duration-200 flex items-center gap-2">
                                    <svg xmlns="http://www.w3.org/2000/svg" class="h-4 w-4" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-8l-4-4m0 0L8 8m4-4v12" /></svg>
                                    Upload
                                </button>
                                <button onclick="deleteWallpaper()" class="bg-red-900/20 hover:bg-red-900/40 text-red-400 text-sm font-semibold py-2 px-4 rounded-lg border border-red-900/40 transition-all duration-200 flex items-center gap-2">
                                    <svg xmlns="http://www.w3.org/2000/svg" class="h-4 w-4" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16" /></svg>
                                    Delete
                                </button>
                            </div>
                        </div>
                        <div id="upload-progress" class="hidden space-y-1">
                            <div class="w-full bg-slate-800 rounded-full h-1.5">
                                <div id="upload-bar" class="bg-primary h-1.5 rounded-full transition-all duration-300" style="width:0%"></div>
                            </div>
                            <p id="upload-label" class="text-[10px] text-slate-500">Uploading...</p>
                        </div>
                    </div>

                    <div id="current-preview" class="hidden space-y-2 pt-2">
                        <h3 class="text-sm font-semibold text-slate-400 uppercase tracking-wider">Preview</h3>
                        <div class="rounded-2xl border border-slate-700 overflow-hidden shadow-2xl shadow-black/60 ring-1 ring-white/10">
                            <img id="wallpaper-preview" data-src="/wallpaper.jpg" class="w-full aspect-[480/320] object-cover bg-slate-800">
                        </div>
                    </div>
                </div>
                <!-- Screensaver Timeout -->
                <div class="space-y-4">
                    <h3 class="text-sm font-semibold text-slate-400 uppercase tracking-wider">Screensaver Timer</h3>
                    <p class="text-xs text-slate-500">How long the screen should be idle before the screensaver activates.</p>
                    <div>
                        <label class="block text-sm font-medium text-slate-300 mb-1.5">Idle Timeout: <span id="timeout-display">2 min</span></label>
                        <input type="range" id="ss_timeout" min="30" max="600" step="30" value="120" oninput="updateTimeoutDisplay(this.value)" class="w-full accent-purple-500">
                        <div class="flex justify-between text-xs text-slate-500 mt-1">
                            <span>30 sec</span><span>10 min</span>
                        </div>
                    </div>
                    <button onclick="saveScreensaverTimeout()" class="bg-slate-700 hover:bg-slate-600 text-white font-semibold py-2 px-5 rounded-lg border border-slate-600 transition-all duration-200 flex items-center gap-2 w-max">
                        <svg xmlns="http://www.w3.org/2000/svg" class="h-4 w-4" viewBox="0 0 20 20" fill="currentColor"><path fill-rule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clip-rule="evenodd" /></svg>
                        Save Timeout
                    </button>
                </div>
            </div>
        </section>

        <!-- Schedules Tab -->
        <section id="tab-schedules" role="tabpanel" aria-labelledby="btn-tab-schedules" tabindex="0" class="tab-pane hidden glass-panel rounded-2xl shadow-2xl border border-borderDark overflow-hidden transition-all duration-500 hover:border-slate-700 hover:shadow-primary/5">
            <div class="border-b border-borderDark px-6 py-4 bg-slate-800/30 flex items-center gap-3">
                <div class="bg-teal-500/20 text-teal-400 p-2 rounded-lg">
                    <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 6v6h4.5m4.5 0a9 9 0 1 1-18 0 9 9 0 0 1 18 0Z" />
                    </svg>
                </div>
                <h2 class="text-lg font-semibold text-slate-100">Schedule Automation</h2>
                <span id="schedule-count" class="bg-teal-500/20 text-teal-300 text-xs font-bold px-2.5 py-1 rounded-full">0</span>
            </div>
            <div class="p-6">
                <div class="flex items-center justify-between mb-4">
                    <p class="text-slate-400 text-sm">Trigger scenes automatically at scheduled times. Max 16 schedules.</p>
                    <div class="flex gap-2">
                        <button onclick="addScheduleCard()" class="bg-teal-600 hover:bg-teal-500 text-white font-semibold py-2 px-5 rounded-lg border border-teal-500 transition-all duration-200 flex items-center gap-2 shadow-lg shadow-teal-500/10">
                            + Add Schedule
                        </button>
                        <button onclick="saveSchedulesAPI()" class="bg-slate-700 hover:bg-slate-600 text-white font-semibold py-2 px-5 rounded-lg border border-slate-600 transition-all duration-200 flex items-center gap-2">
                            <svg xmlns="http://www.w3.org/2000/svg" class="h-4 w-4" viewBox="0 0 20 20" fill="currentColor"><path fill-rule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clip-rule="evenodd" /></svg>
                            Save
                        </button>
                    </div>
                </div>
                <div id="schedules-list" class="grid grid-cols-1 md:grid-cols-2 gap-4"></div>
            </div>
        </section>

        <!-- System & Updates Tab -->
        <section id="tab-system" role="tabpanel" aria-labelledby="btn-tab-system" tabindex="0" class="tab-pane hidden glass-panel rounded-2xl shadow-2xl border border-borderDark overflow-hidden transition-all duration-500 hover:border-slate-700 hover:shadow-primary/5">
            <div class="border-b border-borderDark px-6 py-4 bg-slate-800/30 flex items-center gap-3">
                <div class="bg-indigo-500/20 text-indigo-400 p-2 rounded-lg">
                    <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15" />
                    </svg>
                </div>
                <h2 class="text-lg font-semibold text-slate-100">System & Firmware Updates</h2>
            </div>
            
            <div class="p-6 space-y-6">
                <!-- System Info Dashboard -->
                <div id="sysinfo-card" class="bg-cardDark rounded-2xl border border-slate-700/50 p-6 shadow-inner ring-1 ring-white/5">
                    <div class="flex justify-between items-center mb-4">
                        <h3 class="text-sm font-semibold text-slate-400 uppercase tracking-wider">System Status</h3>
                        <button onclick="loadSysInfo()" class="text-xs text-indigo-400 hover:text-indigo-300 transition-colors">Refresh</button>
                    </div>
                    <div class="grid grid-cols-2 md:grid-cols-3 gap-4" id="sysinfo-grid">
                        <div class="bg-slate-800/60 rounded-lg p-3 border border-slate-700/30">
                            <div class="text-xs text-slate-500 mb-1">WiFi Signal</div>
                            <div class="text-lg font-semibold text-slate-200" id="si-rssi">--</div>
                        </div>
                        <div class="bg-slate-800/60 rounded-lg p-3 border border-slate-700/30">
                            <div class="text-xs text-slate-500 mb-1">Uptime</div>
                            <div class="text-lg font-semibold text-slate-200" id="si-uptime">--</div>
                        </div>
                        <div class="bg-slate-800/60 rounded-lg p-3 border border-slate-700/30">
                            <div class="text-xs text-slate-500 mb-1">Free Heap</div>
                            <div class="text-lg font-semibold text-slate-200" id="si-heap">--</div>
                        </div>
                        <div class="bg-slate-800/60 rounded-lg p-3 border border-slate-700/30">
                            <div class="text-xs text-slate-500 mb-1">Free PSRAM</div>
                            <div class="text-lg font-semibold text-slate-200" id="si-psram">--</div>
                        </div>
                        <div class="bg-slate-800/60 rounded-lg p-3 border border-slate-700/30">
                            <div class="text-xs text-slate-500 mb-1">Storage</div>
                            <div class="text-lg font-semibold text-slate-200" id="si-fs">--</div>
                        </div>
                        <div class="bg-slate-800/60 rounded-lg p-3 border border-slate-700/30">
                            <div class="text-xs text-slate-500 mb-1">IP Address</div>
                            <div class="text-lg font-semibold text-slate-200" id="si-ip">--</div>
                        </div>
                    </div>
                </div>

                <!-- Backup & Restore Card -->
                <div class="bg-cardDark rounded-2xl border border-slate-700/50 p-6 shadow-inner ring-1 ring-white/5">
                    <h3 class="text-sm font-semibold text-slate-400 uppercase tracking-wider mb-2">Backup & Restore</h3>
                    <p class="text-sm text-slate-500 mb-5 border-b border-slate-800 pb-4">
                        Export all settings, devices, scenes, and schedules as a JSON file. Import to restore configuration.
                    </p>
                    <div class="flex flex-col sm:flex-row gap-4">
                        <button onclick="downloadBackup()" class="bg-emerald-600 hover:bg-emerald-500 text-white font-semibold py-2.5 px-6 rounded-lg shadow-lg shadow-emerald-500/20 transition-all duration-300 flex items-center gap-2 justify-center group">
                            <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-4l-4 4m0 0l-4-4m4 4V4" /></svg>
                            Export Backup
                        </button>
                        <label class="bg-amber-600 hover:bg-amber-500 text-white font-semibold py-2.5 px-6 rounded-lg shadow-lg shadow-amber-500/20 transition-all duration-300 flex items-center gap-2 justify-center cursor-pointer group">
                            <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-8l-4-4m0 0L8 8m4-4v12" /></svg>
                            Import Backup
                            <input type="file" id="restore-file" accept=".json" class="hidden" onchange="restoreBackup(this)">
                        </label>
                    </div>
                </div>

                <!-- OTA Update Card -->
                <div class="bg-cardDark rounded-2xl border border-slate-700/50 p-6 shadow-inner ring-1 ring-white/5">
                    <h3 class="text-sm font-semibold text-slate-400 uppercase tracking-wider mb-2">Web OTA Update</h3>
                    <p class="text-sm text-slate-500 mb-6 border-b border-slate-800 pb-4">
                        Upload a compiled `.bin` file to update the panel's firmware over the air without needing to plug it in via USB.
                    </p>
                    
                    <div class="flex flex-col md:flex-row gap-6 items-start md:items-center">
                        <div class="flex-1 w-full">
                            <label class="block">
                                <span class="sr-only">Choose firmware file</span>
                                <input type="file" id="ota-file" accept=".bin" class="block w-full text-sm text-slate-400 file:mr-4 file:py-2.5 file:px-5 file:rounded-lg file:border-0 file:text-sm file:font-semibold file:bg-primary/20 file:text-primary hover:file:bg-primary/30 cursor-pointer transition-all">
                            </label>
                        </div>
                        <button onclick="uploadFirmware()" class="bg-indigo-600 hover:bg-indigo-500 text-white font-semibold py-2.5 px-6 rounded-lg shadow-lg shadow-indigo-500/20 transition-all duration-300 flex items-center gap-2 w-full md:w-auto justify-center group flex-shrink-0">
                            <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5 group-hover:-translate-y-1 transition-transform" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-8l-4-4m0 0L8 8m4-4v12" />
                            </svg>
                            Flash Firmware
                        </button>
                    </div>

                    <!-- OTA Progress -->
                    <div id="ota-progress-container" class="hidden mt-6 space-y-2">
                        <div class="flex justify-between text-xs text-slate-400 font-medium">
                            <span id="ota-label">Preparing update...</span>
                            <span id="ota-percent">0%</span>
                        </div>
                        <div class="w-full bg-slate-900 rounded-full h-2.5 shadow-inner overflow-hidden border border-slate-800">
                            <div id="ota-bar" class="bg-gradient-to-r from-indigo-500 to-purple-500 h-2.5 rounded-full transition-all duration-300 relative" style="width:0%">
                                <div class="absolute inset-0 bg-white/20 w-full animate-pulse"></div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </section>

        <!-- Devices Container Tab -->
        <section id="tab-devices" role="tabpanel" aria-labelledby="btn-tab-devices" tabindex="0" class="tab-pane hidden glass-panel rounded-2xl shadow-2xl border border-borderDark overflow-hidden p-6">
            <div class="flex flex-col sm:flex-row justify-between items-start sm:items-center mb-6 gap-4">
                <div class="flex items-center gap-3">
                    <div class="bg-emerald-500/20 text-emerald-400 p-2 rounded-lg">
                        <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                          <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 11H5m14 0a2 2 0 012 2v6a2 2 0 01-2 2H5a2 2 0 01-2-2v-6a2 2 0 012-2m14 0V9a2 2 0 00-2-2M5 11V9a2 2 0 002-2m0 0V5a2 2 0 012-2h6a2 2 0 012 2v2M7 7h10" />
                        </svg>
                    </div>
                    <h2 class="text-xl font-semibold text-white flex items-center">
                        Hardware Devices 
                        <span id="device-count" class="text-xs font-semibold bg-primary/20 text-primary border border-primary/30 py-1 px-2.5 rounded-full ml-3"></span>
                    </h2>
                </div>
                <button onclick="addDeviceCard()" class="bg-slate-800 hover:bg-slate-700 border border-slate-600 text-slate-200 font-medium py-2 px-6 rounded-2xl flex items-center gap-2 transition-all shadow-md group hover:shadow-primary/20 hover:border-primary/50 duration-300 whitespace-nowrap">
                    <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5 flex-shrink-0 text-slate-400 group-hover:text-primary transition-colors" viewBox="0 0 20 20" fill="currentColor">
                      <path fill-rule="evenodd" d="M10 5a1 1 0 011 1v3h3a1 1 0 110 2h-3v3a1 1 0 11-2 0v-3H6a1 1 0 110-2h3V6a1 1 0 011-1z" clip-rule="evenodd" />
                    </svg>
                    Add Device
                </button>
            </div>

            <!-- Rooms -->
            <div class="mb-6 bg-slate-900/40 border border-slate-700/60 rounded-xl p-4">
                <div class="flex items-center justify-between mb-3">
                    <h3 class="text-sm font-semibold text-slate-200 flex items-center gap-2">
                        <svg xmlns="http://www.w3.org/2000/svg" class="h-4 w-4 text-primary" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M3 12l9-9 9 9M5 10v10h14V10" /></svg>
                        Rooms
                        <span class="text-[11px] font-normal text-slate-500">&mdash; created automatically from the Room field below</span>
                    </h3>
                    <button onclick="saveRoomsAPI()" class="bg-slate-800 hover:bg-slate-700 border border-slate-600 hover:border-primary/50 text-slate-200 text-xs font-medium py-1.5 px-4 rounded-lg transition-all">Save Rooms</button>
                </div>
                <div id="rooms-list" class="grid grid-cols-1 md:grid-cols-2 gap-3"></div>
                <p class="mt-3 text-[11px] text-slate-500">Climate topic is optional. Publish <code class="text-slate-400">{"t":26.5,"h":58}</code> (or <code class="text-slate-400">temperature</code>/<code class="text-slate-400">humidity</code>, or a bare number for temperature) and the room card shows it. Restart the panel after changing a topic so it re-subscribes.</p>
            </div>

            <!-- Device Search -->
            <div class="mb-4">
                <input type="text" id="device-search" placeholder="Search devices by name, room, or topic..." oninput="filterDevices()" class="w-full bg-slate-800/60 border border-slate-700/50 rounded-lg px-4 py-2.5 text-sm text-slate-200 placeholder-slate-500 focus:outline-none focus:border-primary/50 focus:ring-1 focus:ring-primary/30 transition-all">
            </div>

            <!-- CSS Grid for Cards -->
            <div id="devices-list" class="grid grid-cols-1 md:grid-cols-2 xl:grid-cols-3 gap-6">
                <!-- Device cards will be injected here via JS -->
            </div>
        </section>

        <!-- Scenes Tab -->
        <section id="tab-scenes" role="tabpanel" aria-labelledby="btn-tab-scenes" tabindex="0" class="tab-pane hidden glass-panel rounded-2xl shadow-2xl border border-borderDark overflow-hidden p-6">
            <div class="flex flex-col sm:flex-row justify-between items-start sm:items-center mb-6 gap-4">
                <div class="flex items-center gap-3">
                    <div class="bg-amber-500/20 text-amber-400 p-2 rounded-lg">
                        <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                          <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M14.752 11.168l-3.197-2.132A1 1 0 0010 9.87v4.263a1 1 0 001.555.832l3.197-2.132a1 1 0 000-1.664z" />
                          <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
                        </svg>
                    </div>
                    <h2 class="text-xl font-semibold text-white flex items-center">
                        Scene Automation
                        <span id="scene-count" class="text-xs font-semibold bg-amber-500/20 text-amber-400 border border-amber-500/30 py-1 px-2.5 rounded-full ml-3">0</span>
                    </h2>
                </div>
                <div class="flex gap-2">
                    <button onclick="addSceneCard()" class="bg-slate-800 hover:bg-slate-700 border border-slate-600 text-slate-200 font-medium py-2 px-6 rounded-2xl flex items-center gap-2 transition-all shadow-md group hover:shadow-primary/20 hover:border-primary/50 duration-300">
                        <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5 text-slate-400 group-hover:text-primary transition-colors" viewBox="0 0 20 20" fill="currentColor">
                          <path fill-rule="evenodd" d="M10 5a1 1 0 011 1v3h3a1 1 0 110 2h-3v3a1 1 0 11-2 0v-3H6a1 1 0 110-2h3V6a1 1 0 011-1z" clip-rule="evenodd" />
                        </svg>
                        Add Scene
                    </button>
                    <button onclick="saveScenesAPI()" class="bg-gradient-to-r from-primary to-primaryHover hover:from-primaryHover hover:to-orange-600 text-white font-semibold py-2 px-5 rounded-2xl shadow-lg shadow-primary/20 transition-all duration-300 flex items-center gap-2">
                        <svg xmlns="http://www.w3.org/2000/svg" class="h-4 w-4" viewBox="0 0 20 20" fill="currentColor"><path fill-rule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clip-rule="evenodd" /></svg>
                        Save Scenes
                    </button>
                </div>
            </div>
            <p class="text-sm text-slate-500 px-1 mb-4">Scenes let you trigger multiple MQTT commands with one tap on the panel's Scenes tab. Much easier to configure from here than the tiny on-screen keyboard!</p>
            <div id="scenes-list" class="grid grid-cols-1 md:grid-cols-2 gap-6">
            </div>
        </section>

        <!-- Stock Ticker Tab -->
        <section id="tab-stocks" role="tabpanel" aria-labelledby="btn-tab-stocks" tabindex="0" class="tab-pane hidden glass-panel rounded-2xl shadow-2xl border border-borderDark overflow-hidden transition-all duration-500 hover:border-slate-700">
            <div class="border-b border-borderDark px-6 py-4 bg-slate-800/30 flex items-center gap-3">
                <div class="bg-emerald-500/20 text-emerald-400 p-2 rounded-lg">
                    <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M7 12l3-3 3 3 4-4M8 21l4-4 4 4M3 4h18M4 4h16v12a1 1 0 01-1 1H5a1 1 0 01-1-1V4z" />
                    </svg>
                </div>
                <h2 class="text-lg font-semibold text-slate-100">Stock Ticker (Screensaver)</h2>
            </div>
            <div class="p-6 space-y-6">
                <div class="bg-cardDark rounded-2xl border border-slate-700/50 p-6 shadow-inner ring-1 ring-white/5 space-y-5">
                    <p id="stock-desc-p" class="text-sm text-slate-400">Displays up to 3 stock/forex quotes on the Flip Clock screensaver. Powered by <a href="https://twelvedata.com" target="_blank" rel="noopener noreferrer" class="text-primary underline">Twelve Data</a> free tier (800 req/day). US stocks and major forex/metals are usually supported (for example <code class="text-primary">AAPL</code>, <code class="text-primary">XAU/USD</code>). Some symbols such as Thai SET (<code class="text-primary">AOT.BK</code>) may be unavailable on current plan and will show <strong class="text-slate-300">N/A</strong>.</p>

                    <!-- Enable toggle -->
                    <div class="flex items-center justify-between py-2 border-b border-slate-800">
                        <div>
                            <div id="stock-enable-lbl" class="text-sm font-medium text-slate-200">Enable Stock Ticker</div>
                            <div id="stock-enable-sub" class="text-xs text-slate-500 mt-0.5">Shows a compact price bar above the clock on the Flip Clock screensaver</div>
                        </div>
                        <label class="relative inline-flex items-center cursor-pointer">
                            <input type="checkbox" id="stock_enabled" class="sr-only peer">
                            <div class="w-11 h-6 bg-slate-700 peer-focus:outline-none rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-primary"></div>
                        </label>
                    </div>

                    <!-- Symbols -->
                    <div class="grid grid-cols-1 sm:grid-cols-3 gap-4">
                        <div>
                            <label class="block text-sm font-medium text-slate-300 mb-1.5">Asset 1</label>
                            <input type="text" id="stock_s0" placeholder="e.g. AOT.BK" class="w-full bg-slate-900/50 border border-slate-700 rounded-lg px-4 py-2.5 text-slate-200 focus:outline-none focus:ring-2 focus:ring-primary/50 focus:border-primary transition-all placeholder-slate-600 text-sm">
                        </div>
                        <div>
                            <label class="block text-sm font-medium text-slate-300 mb-1.5">Asset 2</label>
                            <input type="text" id="stock_s1" placeholder="e.g. AAPL" class="w-full bg-slate-900/50 border border-slate-700 rounded-lg px-4 py-2.5 text-slate-200 focus:outline-none focus:ring-2 focus:ring-primary/50 focus:border-primary transition-all placeholder-slate-600 text-sm">
                        </div>
                        <div>
                            <label class="block text-sm font-medium text-slate-300 mb-1.5">Asset 3</label>
                            <input type="text" id="stock_s2" placeholder="e.g. XAU/USD" class="w-full bg-slate-900/50 border border-slate-700 rounded-lg px-4 py-2.5 text-slate-200 focus:outline-none focus:ring-2 focus:ring-primary/50 focus:border-primary transition-all placeholder-slate-600 text-sm">
                        </div>
                    </div>

                    <!-- API Key -->
                    <div>
                        <label class="block text-sm font-medium text-slate-300 mb-1.5">Twelve Data API Key</label>
                        <div class="flex gap-3">
                            <input type="password" id="stock_api_key" placeholder="Get free key at twelvedata.com" class="flex-1 bg-slate-900/50 border border-slate-700 rounded-lg px-4 py-2.5 text-slate-200 focus:outline-none focus:ring-2 focus:ring-primary/50 focus:border-primary transition-all placeholder-slate-600 text-sm">
                            <button type="button" onclick="togglePwd('stock_api_key',this)" class="px-3 py-2 rounded-lg border border-slate-700 text-slate-400 hover:text-primary hover:border-primary transition-all">
                                <svg class="h-5 w-5 eye-icon" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15 12a3 3 0 11-6 0 3 3 0 016 0z" /><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M2.458 12C3.732 7.943 7.523 5 12 5c4.478 0 8.268 2.943 9.542 7-1.274 4.057-5.064 7-9.542 7-4.477 0-8.268-2.943-9.542-7z" /></svg>
                            </button>
                        </div>
                    </div>

                    <!-- Save button -->
                    <button onclick="saveStockConfig()" class="bg-gradient-to-r from-primary to-primaryHover hover:from-primaryHover hover:to-orange-600 text-white font-semibold py-2.5 px-8 rounded-lg shadow-lg shadow-primary/20 transition-all duration-300 flex items-center gap-2">
                        <svg xmlns="http://www.w3.org/2000/svg" class="h-4 w-4" viewBox="0 0 20 20" fill="currentColor"><path fill-rule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clip-rule="evenodd" /></svg>
                        Save & Apply
                    </button>
                </div>
            </div>
        </section>
    </main>

    <!-- Room Management Modal -->
    <div id="room-modal" class="fixed inset-0 z-50 hidden items-center justify-center">
        <div class="absolute inset-0 bg-black/70 backdrop-blur-sm" onclick="closeRoomModal()"></div>
        <div class="relative bg-slate-900 border border-slate-700 rounded-2xl shadow-2xl w-full max-w-md mx-4 overflow-hidden animate-fade-in-up">
            <div class="px-6 py-4 border-b border-slate-700/50 flex justify-between items-center">
                <h3 class="text-lg font-semibold text-white flex items-center gap-2">
                    <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5 text-primary" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 21V5a2 2 0 00-2-2H7a2 2 0 00-2 2v16m14 0h2m-2 0h-5m-9 0H3m2 0h5M9 7h1m-1 4h1m4-4h1m-1 4h1m-5 10v-5a1 1 0 011-1h2a1 1 0 011 1v5m-4 0h4" /></svg>
                    Manage Rooms
                </h3>
                <button onclick="closeRoomModal()" class="text-slate-400 hover:text-white p-1 rounded-lg hover:bg-slate-700 transition-colors">
                    <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12" /></svg>
                </button>
            </div>
            <div id="room-modal-body" class="p-4 max-h-[60vh] overflow-y-auto space-y-2"></div>
            <div class="px-6 py-3 border-t border-slate-700/50 bg-slate-800/30">
                <button onclick="closeRoomModal()" class="w-full py-2.5 rounded-lg text-sm font-medium text-slate-400 hover:text-white hover:bg-slate-700 transition-all">Close</button>
            </div>
        </div>
    </div>

    <!-- Toast Notification -->
    <div id="toast" class="fixed bottom-6 right-6 transform translate-y-24 opacity-0 transition-all duration-300 bg-emerald-500 text-white px-6 py-4 rounded-lg shadow-lg shadow-emerald-500/20 flex items-center gap-3 z-50 pointer-events-none">
        <svg xmlns="http://www.w3.org/2000/svg" class="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor">
          <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 12l2 2 4-4m6 2a9 9 0 11-18 0 9 9 0 0118 0z" />
        </svg>
        <div class="font-medium" id="toast-msg">Configuration Saved! Rebooting...</div>
    </div>

<style>
    @keyframes fadeInUp {
        from { opacity: 0; transform: translateY(20px); }
        to { opacity: 1; transform: translateY(0); }
    }
    .animate-fade-in-up {
        animation: fadeInUp 0.5s ease-out forwards;
    }

    @media (prefers-reduced-motion: reduce) {
        *, *::before, *::after {
            animation-duration: 0.01ms !important;
            animation-iteration-count: 1 !important;
            transition-duration: 0.01ms !important;
            scroll-behavior: auto !important;
        }
    }
</style>

<script>
    function escHtml(s) { const d = document.createElement('div'); d.textContent = s; return d.innerHTML; }
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
    }

    function switchTab(tabId, btnElement) {
        // Hide all panes
        document.querySelectorAll('.tab-pane').forEach(el => {
            el.classList.add('hidden');
            el.classList.remove('block');
            el.setAttribute('hidden', 'hidden');
        });
        // Show target pane
        const targetPane = document.getElementById(tabId);
        targetPane.classList.remove('hidden');
        targetPane.classList.add('block');
        targetPane.removeAttribute('hidden');
        
        // Reset all buttons
        document.querySelectorAll('.tab-btn').forEach(btn => {
            btn.classList.remove('bg-primary/10', 'text-primary', 'shadow', 'border-primary/30', 'shadow-primary/20');
            btn.classList.add('text-slate-400', 'border-transparent');
            btn.classList.remove('active');
            btn.setAttribute('aria-selected', 'false');
            btn.setAttribute('tabindex', '-1');
        });
        
        // Active button styles
        btnElement.classList.add('bg-primary/10', 'text-primary', 'shadow', 'border-primary/30', 'shadow-primary/20', 'active');
        btnElement.classList.remove('text-slate-400', 'border-transparent');
        btnElement.setAttribute('aria-selected', 'true');
        btnElement.setAttribute('tabindex', '0');
        btnElement.focus();

        // Lazy-load wallpaper images when tab is opened (sequential to avoid LittleFS crash)
        if (tabId === 'tab-wallpaper') {
            (async () => {
                await lazyLoadImages();
                await loadWallpaperStatus();
            })();
        }
    }

    function initTabKeyboardNavigation() {
        const tabButtons = Array.from(document.querySelectorAll('.tab-btn'));
        if (!tabButtons.length) return;

        tabButtons.forEach((btn, index) => {
            btn.addEventListener('keydown', (e) => {
                const current = tabButtons.indexOf(btn);
                let next = current;
                if (e.key === 'ArrowRight') next = (current + 1) % tabButtons.length;
                else if (e.key === 'ArrowLeft') next = (current - 1 + tabButtons.length) % tabButtons.length;
                else if (e.key === 'Home') next = 0;
                else if (e.key === 'End') next = tabButtons.length - 1;
                else if (e.key === 'Enter' || e.key === ' ') {
                    e.preventDefault();
                    btn.click();
                    return;
                } else {
                    return;
                }
                e.preventDefault();
                tabButtons[next].click();
            });
        });
    }

    let devices = [];
    const MAX_DEVICES = 100;
    
    const DEV_TYPE_OPTIONS = [
    
        { value: 0, text: 'Switch (on/off)' },
    
        { value: 1, text: 'Dimmer (0-100%)' },
    
        { value: 2, text: 'Fan (speed 0-3)' },
    
        { value: 3, text: 'AC (18-30 C)' }
    
    ];
    
    const ICON_OPTIONS = [
        { value: 0, text: "Lamp", svg: '<path stroke-linecap="round" stroke-linejoin="round" d="M12 18v-5.25m0 0a6.01 6.01 0 0 0 1.5-.189m-1.5.189a6.01 6.01 0 0 1-1.5-.189m3.75 7.478a12.06 12.06 0 0 1-4.5 0m3.75 2.383a14.406 14.406 0 0 1-3 0M14.25 18v-.192c0-.983.658-1.823 1.508-2.316a7.5 7.5 0 1 0-7.517 0c.85.493 1.509 1.333 1.509 2.316V18" />' },
        { value: 1, text: "Ceiling Fan", svg: '<path stroke-linecap="round" stroke-linejoin="round" d="M14 12a2 2 0 1 1-4 0 2 2 0 0 1 4 0ZM12 10Q8 9 5 4M14 12Q15 8 20 5M12 14Q16 15 19 20M10 12Q9 16 4 19" />' },
        { value: 2, text: "Power Switch", svg: '<path stroke-linecap="round" stroke-linejoin="round" d="M5.636 5.636a9 9 0 1 0 12.728 0M12 3v9" />' },
        { value: 3, text: "Smart Plug", svg: '<path stroke-linecap="round" stroke-linejoin="round" d="m3.75 13.5 10.5-11.25L12 10.5h8.25L9.75 21.75 12 13.5H3.75Z" />' },
        { value: 4, text: "Thermostat", svg: '<path stroke-linecap="round" stroke-linejoin="round" d="M12 3v2.25m6.364.386-1.591 1.591M21 12h-2.25m-.386 6.364-1.591-1.591M12 18.75V21m-4.773-4.227-1.591 1.591M5.25 12H3m4.227-4.773L5.636 5.636M15.75 12a3.75 3.75 0 1 1-7.5 0 3.75 3.75 0 0 1 7.5 0Z" />' },
        { value: 5, text: "Door Lock", svg: '<path stroke-linecap="round" stroke-linejoin="round" d="M16.5 10.5V6.75a4.5 4.5 0 1 0-9 0v3.75m-.75 11.25h10.5a2.25 2.25 0 0 0 2.25-2.25v-6.75a2.25 2.25 0 0 0-2.25-2.25H6.75a2.25 2.25 0 0 0-2.25 2.25v6.75a2.25 2.25 0 0 0 2.25 2.25Z" />' },
        { value: 6, text: "TV", svg: '<path stroke-linecap="round" stroke-linejoin="round" d="M6 20.25h12m-7.5-3v3m3-3v3m-10.125-3h17.25c.621 0 1.125-.504 1.125-1.125V4.875c0-.621-.504-1.125-1.125-1.125H3.375c-.621 0-1.125.504-1.125 1.125v11.25c0 .621.504 1.125 1.125 1.125Z" />' },
        { value: 7, text: "Garage Door", svg: '<path stroke-linecap="round" stroke-linejoin="round" d="m2.25 12 8.954-8.955c.44-.439 1.152-.439 1.591 0L21.75 12M4.5 9.75v10.125c0 .621.504 1.125 1.125 1.125H9.75v-4.875c0-.621.504-1.125 1.125-1.125h2.25c.621 0 1.125.504 1.125 1.125V21h4.125c.621 0 1.125-.504 1.125-1.125V9.75M8.25 21h8.25" />' },
        { value: 8, text: "LED Strip", svg: '<path stroke-linecap="round" stroke-linejoin="round" d="M9.813 15.904 9 18.75l-.813-2.846a4.5 4.5 0 0 0-3.09-3.09L2.25 12l2.846-.813a4.5 4.5 0 0 0 3.09-3.09L9 5.25l.813 2.846a4.5 4.5 0 0 0 3.09 3.09L15.75 12l-2.846.813a4.5 4.5 0 0 0-3.09 3.09ZM18.259 8.715 18 9.75l-.259-1.035a3.375 3.375 0 0 0-2.455-2.456L14.25 6l1.036-.259a3.375 3.375 0 0 0 2.455-2.456L18 2.25l.259 1.035a3.375 3.375 0 0 0 2.456 2.456L21.75 6l-1.035.259a3.375 3.375 0 0 0-2.456 2.456ZM16.894 20.567 16.5 21.75l-.394-1.183a2.25 2.25 0 0 0-1.423-1.423L13.5 18.75l1.183-.394a2.25 2.25 0 0 0 1.423-1.423l.394-1.183.394 1.183a2.25 2.25 0 0 0 1.423 1.423l1.183.394-1.183.394a2.25 2.25 0 0 0-1.423 1.423Z" />' },
        { value: 9, text: "Generic", svg: '<path stroke-linecap="round" stroke-linejoin="round" d="m21 7.5-2.25-1.313M21 7.5v2.25m0-2.25-2.25 1.313M3 7.5l2.25-1.313M3 7.5l2.25 1.313M3 7.5v2.25m9 3 2.25-1.313M12 12.75l-2.25-1.313M12 12.75V15m0 6.75 2.25-1.313M12 21.75V19.5m0 2.25-2.25-1.313m0-16.875L12 2.25l2.25 1.313M21 14.25v2.25l-2.25 1.313m-13.5 0L3 16.5v-2.25" />' }
    ];

    // Load data-src images sequentially to avoid concurrent LittleFS access crash
    async function lazyLoadImages() {
        const imgs = document.querySelectorAll('img[data-src]');
        for (const img of imgs) {
            if (img.id === 'wallpaper-preview') continue;
            const url = img.dataset.src;
            let ok = await new Promise(r => {
                img.onload = () => r(true);
                img.onerror = () => r(false);
                img.src = url;
            });
            if (!ok) {
                ok = await new Promise(r => {
                    setTimeout(() => {
                        img.onload = () => r(true);
                        img.onerror = () => r(false);
                        img.src = url + '?r=' + Date.now();
                    }, 800);
                });
            }
            if (ok) { img.removeAttribute('data-src'); }
            else { img.style.opacity = '0'; img.parentElement.style.background = 'linear-gradient(135deg, #1e1b4b 0%, #312e81 50%, #1e1b4b 100%)'; }
        }
    }

    // ---- Language switcher ----
    async function setLang(code) {
        try {
            await fetch('/api/lang/set', {
                method: 'POST',
                headers: {'Content-Type':'application/json'},
                body: JSON.stringify({code})
            });
        } catch(e) { /* best-effort */ }
        location.reload();
    }

    function initLangButtons(activeLang) {
        document.querySelectorAll('.lang-btn').forEach(btn => {
            const code = btn.id.replace('lang-', '');
            const active = (code === activeLang);
            btn.setAttribute('aria-pressed', active ? 'true' : 'false');
        });
    }

    let keepAliveTimer = null;
    function startPanelKeepAlive() {
        if (keepAliveTimer) clearInterval(keepAliveTimer);
        keepAliveTimer = setInterval(() => {
            if (document.visibilityState !== 'visible') return;
            fetch('/api/lang', { cache: 'no-store' }).catch(() => {});
        }, 15000);
    }

    document.addEventListener('visibilitychange', () => {
        if (document.visibilityState === 'visible') {
            fetch('/api/lang', { cache: 'no-store' }).catch(() => {});
        }
    });

    // Fetch all data sequentially on load to avoid overwhelming ESP32
    window.onload = async function() {
        startPanelKeepAlive();
        initTabKeyboardNavigation();
        // Detect current lang from /api/lang and highlight switcher
        fetch('/api/lang').then(r=>r.json()).then(dict=>{
            const lang = (dict && dict._lang) ? dict._lang : (dict && Object.keys(dict).length>1 ? 'th' : 'en');
            initLangButtons(lang);
        }).catch(()=>initLangButtons('en'));
        document.getElementById('devices-list').innerHTML = '<div class="col-span-full text-center py-12 text-slate-500 flex flex-col items-center"><svg class="animate-spin h-8 w-8 text-primary mb-4" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24"><circle class="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" stroke-width="4"></circle><path class="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path></svg>Loading panel configuration...</div>';

        try {
            const response = await fetch('/api/config');
            if (!response.ok) throw new Error('HTTP ' + response.status);
            const data = await response.json();
            document.getElementById('wifi_ssid').value = data.wifi_ssid || '';
            document.getElementById('wifi_pass').value = data.wifi_pass || '';
            document.getElementById('mqtt_srv').value = data.mqtt_srv || '';
            document.getElementById('mqtt_port').value = data.mqtt_port || 1883;
            document.getElementById('mqtt_usr').value = data.mqtt_usr || '';
            document.getElementById('mqtt_pwd').value = data.mqtt_pwd || '';
            document.getElementById('weather_city').value = data.weather_city || '';
            setCityCoords(data.weather_lat || 0, data.weather_lon || 0);
            document.getElementById('panel_title').value = data.panel_title || 'Hero Home Panel';
            document.getElementById('theme_dark').value = data.theme_dark !== undefined ? data.theme_dark.toString() : 'true';
            document.getElementById('large_tiles').value = data.large_tiles !== undefined ? data.large_tiles.toString() : 'false';
            document.getElementById('time_24h').value = data.time_24h !== undefined ? data.time_24h.toString() : 'true';
            document.getElementById('gmt_offset').value = data.gmt_offset !== undefined ? data.gmt_offset.toString() : '7';
            document.getElementById('web_user').value = data.web_user || 'admin';
            document.getElementById('web_pass').value = data.web_pass || 'admin';
            devices = data.devices || [];
            renderDevices();
            loadRoomsAPI(); // rooms come from their own endpoint
            // Sync lang switcher with saved config
            if (data.lang) initLangButtons(data.lang);
            if (data.ss_timeout) {
                document.getElementById('ss_timeout').value = data.ss_timeout;
                updateTimeoutDisplay(data.ss_timeout);
            }
        } catch(err) {
            console.error('Failed to fetch config:', err);
            document.getElementById('devices-list').innerHTML = '<div class="col-span-full text-center py-12 text-red-400">Failed to load configuration. Make sure Panel is online.</div>';
        }
        await loadScenes();
        await loadSchedules();
        await loadSysInfo();
        await loadStockConfig();
    };

    function filterDevices() {
        const q = (document.getElementById('device-search').value || '').toLowerCase();
        const cards = document.getElementById('devices-list').children;
        for (let i = 0; i < cards.length; i++) {
            const dev = devices[i];
            if (!dev) { cards[i].style.display = ''; continue; }
            const text = (dev.name + ' ' + dev.room + ' ' + dev.state_topic + ' ' + dev.cmnd_topic).toLowerCase();
            cards[i].style.display = (!q || text.includes(q)) ? '' : 'none';
        }
    }

    function getRoomOptions(currentRoom) {
        const rooms = [...new Set(devices.map(d => d.room).filter(r => r && r.trim()))];
        if (currentRoom && !rooms.includes(currentRoom)) rooms.push(currentRoom);
        rooms.sort();
        let html = rooms.map(r => `<option value="${r}" ${r === currentRoom ? 'selected' : ''}>${r}</option>`).join('');
        html += '<option value="__add_new__">+ Add new room...</option>';
        html += '<option value="__del_room__">Manage rooms...</option>';
        return html;
    }

    function onRoomChange(sel, index) {
        if (sel.value === '__add_new__') {
            const newRoom = prompt('Enter new room name:');
            if (newRoom && newRoom.trim()) {
                updateDevice(index, 'room', newRoom.trim());
            }
            renderDevices();
        } else if (sel.value === '__del_room__') {
            manageRooms();
            renderDevices();
        } else {
            updateDevice(index, 'room', sel.value);
        }
    }

    function manageRooms() {
        const rooms = [...new Set(devices.map(d => d.room).filter(r => r && r.trim()))];
        rooms.sort();
        const modal = document.getElementById('room-modal');
        const body = document.getElementById('room-modal-body');
        if (rooms.length === 0) {
            body.innerHTML = '<div class="text-center py-8 text-slate-500"><svg xmlns="http://www.w3.org/2000/svg" class="h-12 w-12 mx-auto mb-3 opacity-40" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5" d="M19 21V5a2 2 0 00-2-2H7a2 2 0 00-2 2v16m14 0h2m-2 0h-5m-9 0H3m2 0h5M9 7h1m-1 4h1m4-4h1m-1 4h1m-5 10v-5a1 1 0 011-1h2a1 1 0 011 1v5m-4 0h4" /></svg>No rooms created yet</div>';
        } else {
            body.innerHTML = rooms.map(r => {
                const count = devices.filter(d => d.room === r).length;
                return `<div class="flex items-center justify-between bg-slate-800/60 border border-slate-700/50 rounded-lg px-4 py-3 group hover:border-slate-600 transition-all">
                    <div class="flex items-center gap-3 min-w-0">
                        <div class="bg-primary/10 text-primary p-2 rounded-lg flex-shrink-0">
                            <svg xmlns="http://www.w3.org/2000/svg" class="h-4 w-4" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M3 12l2-2m0 0l7-7 7 7M5 10v10a1 1 0 001 1h3m10-11l2 2m-2-2v10a1 1 0 01-1 1h-3m-4 0h4" /></svg>
                        </div>
                        <div class="min-w-0">
                            <div class="font-medium text-slate-200 truncate">${r}</div>
                            <div class="text-xs text-slate-500">${count} devices</div>
                        </div>
                    </div>
                    <div class="flex gap-1.5">
                        <button onclick="renameRoom('${r.replace(/'/g, "\\\\'")}', this)" class="flex items-center gap-1.5 text-xs font-medium text-slate-500 hover:text-primary bg-slate-700/50 hover:bg-primary/10 border border-transparent hover:border-primary/30 px-3 py-1.5 rounded-lg transition-all opacity-60 group-hover:opacity-100">
                            <svg xmlns="http://www.w3.org/2000/svg" class="h-3.5 w-3.5" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M11 5H6a2 2 0 00-2 2v11a2 2 0 002 2h11a2 2 0 002-2v-5m-1.414-9.414a2 2 0 112.828 2.828L11.828 15H9v-2.828l8.586-8.586z" /></svg>
                            Rename
                        </button>
                        <button onclick="deleteRoom('${r.replace(/'/g, "\\\\'")}', this)" class="flex items-center gap-1.5 text-xs font-medium text-slate-500 hover:text-red-400 bg-slate-700/50 hover:bg-red-400/10 border border-transparent hover:border-red-400/30 px-3 py-1.5 rounded-lg transition-all opacity-60 group-hover:opacity-100">
                            <svg xmlns="http://www.w3.org/2000/svg" class="h-3.5 w-3.5" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16" /></svg>
                            Delete
                        </button>
                    </div>
                </div>`;
            }).join('');
        }
        modal.classList.remove('hidden');
        modal.classList.add('flex');
    }

    function closeRoomModal() {
        const modal = document.getElementById('room-modal');
        modal.classList.add('hidden');
        modal.classList.remove('flex');
    }

    function deleteRoom(roomName, btn) {
        const count = devices.filter(d => d.room === roomName).length;
        const card = btn.closest('div.flex');
        card.style.transition = 'all 0.3s';
        card.style.opacity = '0';
        card.style.transform = 'translateX(20px)';
        setTimeout(() => {
            devices.forEach(d => { if (d.room === roomName) d.room = ''; });
            card.remove();
            renderDevices();
            showToast('Room "' + roomName + '" deleted (' + count + ' devices unassigned)');
            const body = document.getElementById('room-modal-body');
            if (!body.querySelector('div.flex')) {
                body.innerHTML = '<div class="text-center py-8 text-slate-500">All rooms deleted</div>';
            }
        }, 300);
    }

    function renameRoom(oldName, btn) {
        const newName = prompt('Rename room "' + oldName + '" to:', oldName);
        if (!newName || !newName.trim() || newName.trim() === oldName) return;
        const trimmed = newName.trim();
        const count = devices.filter(d => d.room === oldName).length;
        devices.forEach(d => { if (d.room === oldName) d.room = trimmed; });
        renderDevices();
        closeRoomModal();
        manageRooms();
        showToast('Renamed "' + oldName + '" -> "' + trimmed + '" (' + count + ' devices)');
    }

    // ── Weather city picker ──────────────────────────────────────────────
    // Queries Open-Meteo's geocoding API straight from the browser (it sends
    // access-control-allow-origin: *), so the panel never proxies it — that
    // keeps the request off the ESP32's network task, which runs under a 5 s
    // watchdog.
    //
    // Picking a result pins its coordinates; typing by hand clears them, which
    // tells the firmware to geocode the name at boot the way it always did.
    // Without that, editing the city after a pick would leave the panel
    // fetching weather for the previous place.
    let cityPickTimer = null;

    function setCityCoords(lat, lon) {
        document.getElementById('weather_lat').value = (lat || lon) ? lat : '';
        document.getElementById('weather_lon').value = (lat || lon) ? lon : '';
        renderCityPin();
    }

    // Coordinates are shown, not hidden, so their state is never a surprise —
    // which is why typing a city name no longer wipes them. Out-of-range values
    // are called out here rather than silently zeroed by the firmware.
    function renderCityPin() {
        const pin = document.getElementById('city-pinned');
        const lat = parseFloat(document.getElementById('weather_lat').value);
        const lon = parseFloat(document.getElementById('weather_lon').value);
        if (isNaN(lat) && isNaN(lon)) {
            pin.textContent = 'no coordinates — the panel will look the city up by name';
            pin.className = 'text-[10px] text-slate-500';
            return;
        }
        const bad = isNaN(lat) || isNaN(lon) ||
                    lat < -90 || lat > 90 || lon < -180 || lon > 180;
        if (bad) {
            pin.textContent = 'latitude must be -90..90 and longitude -180..180';
            pin.className = 'text-[10px] text-red-400';
        } else {
            pin.textContent = `pinned to ${lat.toFixed(4)}, ${lon.toFixed(4)} — the city name is just a label`;
            pin.className = 'text-[10px] text-emerald-400';
        }
    }

    function onCoordInput() { renderCityPin(); }

    function clearCityCoords() {
        setCityCoords(0, 0);
    }

    function hideCityResults() {
        document.getElementById('city-results').classList.add('hidden');
    }

    function onCityInput() {
        // Deliberately does not clear the coordinates: they are visible now, so
        // a name that disagrees with a pinned location is on screen rather than
        // hidden. "Look up by name instead" clears them explicitly.
        clearTimeout(cityPickTimer);
        const q = document.getElementById('weather_city').value.trim();
        if (q.length < 2) { hideCityResults(); return; }
        cityPickTimer = setTimeout(() => searchCity(q), 300);
    }

    async function searchCity(q) {
        const box = document.getElementById('city-results');
        try {
            const r = await fetch(
                'https://geocoding-api.open-meteo.com/v1/search?count=5&format=json&name=' +
                encodeURIComponent(q));
            if (!r.ok) { hideCityResults(); return; }
            const results = (await r.json()).results || [];
            if (!results.length) { hideCityResults(); return; }

            box.innerHTML = results.map((c, i) => {
                // Region and country disambiguate same-named places, which is
                // the whole point of picking from a list.
                const where = [c.admin1, c.country].filter(Boolean).join(', ');
                return `<button type="button" data-i="${i}"
                    class="w-full text-left px-4 py-2 hover:bg-slate-700/60 transition-colors border-b border-slate-700/40 last:border-0">
                    <span class="text-sm text-slate-100">${escHtml(c.name)}</span>
                    <span class="text-xs text-slate-500 ml-2">${escHtml(where)}</span>
                </button>`;
            }).join('');

            box.querySelectorAll('button').forEach(btn => {
                btn.onclick = () => {
                    const c = results[+btn.dataset.i];
                    document.getElementById('weather_city').value = c.name;
                    setCityCoords(c.latitude, c.longitude);
                    hideCityResults();
                };
            });
            box.classList.remove('hidden');
        } catch (e) {
            hideCityResults(); // offline browser: the field still works as text
        }
    }

    document.addEventListener('click', (e) => {
        if (!e.target.closest('#city-field')) hideCityResults();
    });

    let ROOMS = [];

    function renderRooms() {
        const list = document.getElementById('rooms-list');
        if (!list) return;
        if (!ROOMS.length) {
            list.innerHTML = '<p class="text-xs text-slate-500 col-span-full">No rooms yet &mdash; give a device a Room name and save.</p>';
            return;
        }
        // "Auto" keeps the firmware picking the icon from whatever devices the
        // room holds, which is the default and usually the right answer.
        const iconOpts = [{ value: -1, text: 'Auto' }].concat(ICON_OPTIONS.map(o => ({ value: o.value, text: o.text })));
        list.innerHTML = ROOMS.map((rm, i) => `
            <div class="bg-slate-800/40 border border-slate-700/50 rounded-lg p-3">
                <div class="flex items-center justify-between mb-2">
                    <span class="text-sm font-medium text-slate-100">${escHtml(rm.name)}</span>
                    <span class="text-[11px] text-slate-500">${rm.devices} device${rm.devices == 1 ? '' : 's'}${rm.climate_valid ? ` &middot; ${rm.temp.toFixed(1)}&deg; ${rm.hum}%` : ''}</span>
                </div>
                <label class="block text-[10px] font-semibold text-slate-500 uppercase tracking-wider mb-1">Icon</label>
                <select class="w-full mb-2 bg-slate-900/40 border border-slate-700/80 rounded-lg px-3 py-1.5 text-xs text-slate-200 focus:outline-none focus:border-primary"
                    onchange="ROOMS[${i}].icon_type = parseInt(this.value)">
                    ${iconOpts.map(o => `<option value="${o.value}" ${(rm.icon_type == null ? -1 : rm.icon_type) == o.value ? 'selected' : ''}>${o.text}</option>`).join('')}
                </select>
                <label class="block text-[10px] font-semibold text-slate-500 uppercase tracking-wider mb-1">Climate Topic</label>
                <input type="text" value="${escHtml(rm.climate_topic || '')}" onchange="ROOMS[${i}].climate_topic = this.value"
                    placeholder="Optional: home/living/climate"
                    class="w-full bg-slate-900/40 border border-slate-700/80 rounded-lg px-3 py-1.5 text-xs text-slate-300 font-mono focus:outline-none focus:border-primary placeholder-slate-600">
            </div>`).join('');
    }

    async function loadRoomsAPI() {
        try {
            const r = await fetch('/api/rooms');
            if (!r.ok) return;
            ROOMS = (await r.json()).rooms || [];
            renderRooms();
        } catch (e) { console.error('rooms load', e); }
    }

    async function saveRoomsAPI() {
        try {
            const r = await fetch('/api/rooms', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ rooms: ROOMS })
            });
            showToast(r.ok ? 'Rooms saved. Restart the panel to re-subscribe.' : 'Save failed', !r.ok);
        } catch (e) { showToast('Save failed', true); }
    }

    function renderDevices() {
        const list = document.getElementById('devices-list');
        list.innerHTML = '';
        
        document.getElementById('device-count').innerText = `Devices: ${devices.length}`;

        devices.forEach((dev, index) => {
            let optionsHtml = ICON_OPTIONS.map(opt => 
                `<option value="${opt.value}" ${dev.icon_type == opt.value ? 'selected' : ''}>${opt.text}</option>`
            ).join('');

            // Get SVG for currently selected icon
            let currentIcon = ICON_OPTIONS.find(opt => opt.value == (dev.icon_type || 0)) || ICON_OPTIONS[0];

            list.innerHTML += `
                <div class="bg-cardDark rounded-2xl shadow-lg border border-borderDark overflow-hidden flex flex-col group relative transition-all duration-500 hover:shadow-2xl hover:border-slate-500 hover:-translate-y-2 hover:bg-slate-800/80 ring-1 ring-white/5">
                    <div class="px-5 py-3.5 border-b border-borderDark bg-slate-800/40 flex justify-between items-center group-hover:bg-slate-800/70 transition-colors">
                        <div class="font-semibold text-slate-200 flex items-center gap-2">
                            <span class="bg-primary/20 text-primary border border-primary/30 text-xs px-2 py-0.5 rounded shadow-sm font-mono">ID:${index + 1}</span>
                            <span class="truncate max-w-[200px]" title="${escHtml(dev.name)}">${escHtml(dev.name) || 'Unnamed'}</span>
                        </div>
                        <button onclick="removeDevice(${index})" class="text-slate-500 hover:text-red-400 p-1.5 rounded-lg hover:bg-red-400/10 transition-colors flex items-center gap-1 text-sm font-medium" title="Delete Device">
                            <svg xmlns="http://www.w3.org/2000/svg" class="h-4 w-4" viewBox="0 0 20 20" fill="currentColor">
                              <path fill-rule="evenodd" d="M9 2a1 1 0 00-.894.553L7.382 4H4a1 1 0 000 2v10a2 2 0 002 2h8a2 2 0 002-2V6a1 1 0 100-2h-3.382l-.724-1.447A1 1 0 0011 2H9zM7 8a1 1 0 012 0v6a1 1 0 11-2 0V8zm5-1a1 1 0 00-1 1v6a1 1 0 102 0V8a1 1 0 00-1-1z" clip-rule="evenodd" />
                            </svg> Delete
                        </button>
                    </div>
                    
                    <div class="p-5 flex-1 space-y-4">
                        <div class="flex items-center gap-3 mb-3">
                            <div class="bg-slate-800 border border-borderDark p-3 rounded-lg flex-shrink-0 text-primary shadow-inner">
                                <svg xmlns="http://www.w3.org/2000/svg" class="h-7 w-7" fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="1.5">
                                   ${currentIcon.svg}
                                </svg>
                            </div>
                            <div class="flex-1 min-w-0">
                                <label class="block text-xs font-semibold text-slate-500 uppercase tracking-wider mb-1.5">Device Name</label>
                                <input class="w-full bg-slate-900/40 border border-slate-700/80 rounded-lg px-3 py-2 text-sm text-slate-200 focus:outline-none focus:ring-2 focus:ring-primary/40 focus:border-primary transition-all shadow-inner" 
                                    type="text" value="${escHtml(dev.name)}" onchange="updateDevice(${index}, 'name', this.value)" placeholder="e.g. Desk Lamp">
                            </div>
                        </div>
                        <div>
                            <label class="block text-xs font-semibold text-slate-500 uppercase tracking-wider mb-1.5">Room</label>
                            <div class="relative">
                                <select class="w-full bg-slate-900/40 border border-slate-700/80 rounded-lg px-3 py-2 text-sm text-slate-200 appearance-none focus:outline-none focus:ring-2 focus:ring-primary/40 focus:border-primary cursor-pointer shadow-inner" 
                                    onchange="onRoomChange(this, ${index})">
                                    ${getRoomOptions(dev.room || 'Living Room')}
                                </select>
                                <div class="pointer-events-none absolute inset-y-0 right-0 flex items-center px-3 text-slate-400">
                                    <svg class="h-4 w-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 9l-7 7-7-7"></path></svg>
                                </div>
                            </div>
                        </div>
                        
                        <div>
                            <label class="block text-xs font-semibold text-slate-500 uppercase tracking-wider mb-1.5 flex items-center gap-1">
                                <svg xmlns="http://www.w3.org/2000/svg" class="h-3.5 w-3.5" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M13 10V3L4 14h7v7l9-11h-7z" /></svg>
                                State Topic
                            </label>
                            <input class="w-full bg-slate-900/40 border border-slate-700/80 rounded-lg px-4 py-2.5 text-[13px] text-slate-300 font-mono focus:outline-none focus:ring-2 focus:ring-primary/40 focus:border-primary transition-all placeholder-slate-600 shadow-inner" 
                                type="text" value="${escHtml(dev.state_topic)}" onchange="updateDevice(${index}, 'state_topic', this.value)" placeholder="homebridge/.../stat">
                        </div>
                        
                        <div>
                            <label class="block text-xs font-semibold text-slate-500 uppercase tracking-wider mb-1.5 flex items-center gap-1">
                                <svg xmlns="http://www.w3.org/2000/svg" class="h-3.5 w-3.5" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M8 7h12m0 0l-4-4m4 4l-4 4m0 6H4m0 0l4 4m-4-4l4-4" /></svg>
                                Command Topic
                            </label>
                            <input class="w-full bg-slate-900/40 border border-slate-700/80 rounded-lg px-4 py-2.5 text-[13px] text-slate-300 font-mono focus:outline-none focus:ring-2 focus:ring-primary/40 focus:border-primary transition-all placeholder-slate-600 shadow-inner" 
                                type="text" value="${escHtml(dev.cmnd_topic)}" onchange="updateDevice(${index}, 'cmnd_topic', this.value)" placeholder="homebridge/.../set">
                        </div>
                        
                        <div>
                            <label class="block text-xs font-semibold text-slate-500 uppercase tracking-wider mb-1.5 flex items-center gap-1">
                                <svg xmlns="http://www.w3.org/2000/svg" class="h-3.5 w-3.5" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 3v1m0 16v1m9-9h-1M4 12H3m15.364 6.364l-.707-.707M6.343 6.343l-.707-.707m12.728 0l-.707.707M6.343 17.657l-.707.707M16 12a4 4 0 11-8 0 4 4 0 018 0z" /></svg>
                                Dimmer Topic
                            </label>
                            <input class="w-full bg-slate-900/40 border border-slate-700/80 rounded-lg px-4 py-2.5 text-[13px] text-slate-300 font-mono focus:outline-none focus:ring-2 focus:ring-primary/40 focus:border-primary transition-all placeholder-slate-600 shadow-inner" 
                                type="text" value="${escHtml(dev.dimmer_topic || '')}" onchange="updateDevice(${index}, 'dimmer_topic', this.value)" placeholder="Optional: homebridge/.../dim">
                        </div>
                        
                        <div>
                            <label class="block text-xs font-semibold text-slate-500 uppercase tracking-wider mb-1.5 flex items-center gap-1">
                                <svg xmlns="http://www.w3.org/2000/svg" class="h-3.5 w-3.5" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 6h16M4 12h16M4 18h7" /></svg>
                                Control Type
                            </label>
                            <div class="relative">
                                <select class="w-full bg-slate-900/40 border border-slate-700/80 rounded-lg px-4 py-2.5 text-sm text-slate-200 appearance-none focus:outline-none focus:ring-2 focus:ring-primary/40 focus:border-primary cursor-pointer shadow-inner"
                                    onchange="updateDevice(${index}, 'dev_type', parseInt(this.value))">
                                    ${DEV_TYPE_OPTIONS.map(o => `<option value="${o.value}" ${(dev.dev_type || 0) == o.value ? 'selected' : ''}>${o.text}</option>`).join('')}
                                </select>
                                <div class="pointer-events-none absolute inset-y-0 right-0 flex items-center px-3 text-slate-400">
                                    <svg class="h-4 w-4" fill="none" stroke="currentColor" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 9l-7 7-7-7"></path></svg>
                                </div>
                            </div>
                            <p class="mt-1.5 text-[11px] text-slate-500">Dimmer, Fan and AC send their level on the Dimmer Topic: 0-100, speed 0-3, or 18-30&deg;C.</p>
                        </div>

                        <div>
                            <label class="block text-xs font-semibold text-slate-500 uppercase tracking-wider mb-1.5 flex items-center gap-1">
                                <svg xmlns="http://www.w3.org/2000/svg" class="h-3.5 w-3.5 outline-none" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 16l4.586-4.586a2 2 0 012.828 0L16 16m-2-2l1.586-1.586a2 2 0 012.828 0L20 14m-6-6h.01M6 20h12a2 2 0 002-2V6a2 2 0 00-2-2H6a2 2 0 00-2 2v12a2 2 0 002 2z" /></svg>
                                Device Icon
                            </label>
                            <div class="relative">
                                <select class="w-full bg-slate-900/40 border border-slate-700/80 rounded-lg px-4 py-2.5 text-sm text-slate-200 appearance-none focus:outline-none focus:ring-2 focus:ring-primary/40 focus:border-primary cursor-pointer shadow-inner" 
                                    onchange="updateDevice(${index}, 'icon_type', parseInt(this.value)); setTimeout(renderDevices, 0);">
                                    ${optionsHtml}
                                </select>
                                <div class="pointer-events-none absolute inset-y-0 right-0 flex items-center px-3 text-slate-400">
                                    <svg class="h-4 w-4" fill="none" stroke="currentColor" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 9l-7 7-7-7"></path></svg>
                                </div>
                            </div>
                        </div>
                        
                        <div class="pt-2">
                            <label class="flex items-center gap-3 cursor-pointer group">
                                <div class="relative">
                                    <input type="checkbox" ${dev.is_favorite ? 'checked' : ''} onchange="updateDevice(${index}, 'is_favorite', this.checked)" class="sr-only peer">
                                    <div class="h-5 w-9 bg-slate-700 peer-focus:outline-none rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-4 after:w-4 after:transition-all peer-checked:bg-primary shadow-inner"></div>
                                </div>
                                <span class="text-sm font-medium text-slate-300 group-hover:text-primary transition-colors flex items-center gap-1.5">
                                    <svg xmlns="http://www.w3.org/2000/svg" class="h-4 w-4 ${dev.is_favorite ? 'text-amber-400 fill-amber-400' : 'text-slate-500'}" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                                      <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M11.049 2.927c.3-.921 1.603-.921 1.902 0l1.519 4.674a1 1 0 00.95.69h4.915c.969 0 1.371 1.24.588 1.81l-3.976 2.888a1 1 0 00-.363 1.118l1.518 4.674c.3.922-.755 1.688-1.538 1.118l-3.976-2.888a1 1 0 00-1.176 0l-3.976 2.888c-.783.57-1.838-.197-1.538-1.118l1.518-4.674a1 1 0 00-.363-1.118l-3.976-2.888c-.783-.57-.38-1.81.588-1.81h4.914a1 1 0 00.951-.69l1.519-4.674z" />
                                    </svg>
                                    Favorite on Home Screen
                                </span>
                            </label>
                        </div>
                            </div>
                        </div>
                    </div>
                </div>
            `;
        });
        
        // Add an "AddNewCard" placeholder if space exists
        if (devices.length < MAX_DEVICES) {
           list.innerHTML += `
                <div onclick="addDeviceCard()" class="border-2 border-dashed border-slate-600/60 rounded-2xl flex flex-col items-center justify-center text-slate-500 hover:text-primary hover:border-primary hover:bg-primary/5 cursor-pointer transition-all duration-300 min-h-[350px] group shadow-sm hover:shadow-md hover:-translate-y-1">
                    <div class="bg-slate-800 p-4 rounded-full mb-4 group-hover:bg-primary/10 transition-colors shadow-inner">
                        <svg xmlns="http://www.w3.org/2000/svg" class="h-8 w-8 text-slate-400 group-hover:text-primary transition-colors" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                          <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 4v16m8-8H4" />
                        </svg>
                    </div>
                    <span class="font-semibold text-lg">Add New Device</span>
                </div>
            `;
        }
    }

    function updateDevice(index, field, value) {
        devices[index][field] = value;
    }

    function removeDevice(index) {
        if(confirm("Are you sure you want to delete this device? It cannot be undone without saving.")) {
            devices.splice(index, 1);
            renderDevices();
        }
    }

    function addDeviceCard() {
        if(devices.length >= MAX_DEVICES) {
            alert("Maximum system capacity (100 devices) reached.");
            return;
        }
        devices.push({
            name: "New Device",
            room: "Living Room",
            state_topic: "homebridge//stat",
            cmnd_topic: "homebridge//set",
            dimmer_topic: "",
            dev_type: 0,
            icon_type: 2
        });
        // Scroll to bottom after state update
        renderDevices();
        window.scrollTo({ top: document.body.scrollHeight, behavior: 'smooth' });
    }

    function showToast(message, isError=false) {
        const toast = document.getElementById("toast");
        const msg = document.getElementById("toast-msg");
        msg.innerText = message;
        
        if (isError) {
            toast.classList.replace('bg-emerald-500', 'bg-red-500');
            toast.classList.replace('shadow-emerald-500/20', 'shadow-red-500/20');
        } else {
            toast.classList.replace('bg-red-500', 'bg-emerald-500');
            toast.classList.replace('shadow-red-500/20', 'shadow-emerald-500/20');
        }

        toast.classList.remove("translate-y-24", "opacity-0");
        toast.classList.add("translate-y-0", "opacity-100");
        
        setTimeout(() => {
            toast.classList.remove("translate-y-0", "opacity-100");
            toast.classList.add("translate-y-24", "opacity-0");
        }, 4000);
    }

    function saveConfiguration() {
        // Client-side validation
        const ssid = document.getElementById('wifi_ssid').value.trim();
        const mqttSrv = document.getElementById('mqtt_srv').value.trim();
        const webUser = document.getElementById('web_user').value.trim();
        const webPass = document.getElementById('web_pass').value;

        if (!ssid) { showToast('WiFi SSID cannot be empty.', true); return; }
        if (ssid.length > 32) { showToast('WiFi SSID too long (max 32 chars).', true); return; }
        if (mqttSrv && !/^(\d{1,3}\.){3}\d{1,3}$|^[a-zA-Z0-9.-]+$/.test(mqttSrv)) {
            showToast('Invalid MQTT server address.', true); return;
        }
        if (!webUser || webUser.length < 1) { showToast('Web username cannot be empty.', true); return; }
        if (webPass && webPass !== '********' && webPass.length < 4) {
            showToast('Web password must be at least 4 characters.', true); return;
        }

        const payload = {
            wifi_ssid: document.getElementById('wifi_ssid').value,
            wifi_pass: document.getElementById('wifi_pass').value,
            mqtt_srv: document.getElementById('mqtt_srv').value,
            mqtt_port: parseInt(document.getElementById('mqtt_port').value) || 1883,
            mqtt_usr: document.getElementById('mqtt_usr').value,
            mqtt_pwd: document.getElementById('mqtt_pwd').value,
            weather_city: document.getElementById('weather_city').value,
            weather_lat: parseFloat(document.getElementById('weather_lat').value) || 0,
            weather_lon: parseFloat(document.getElementById('weather_lon').value) || 0,
            panel_title: document.getElementById('panel_title').value,
            theme_dark: document.getElementById('theme_dark').value === 'true',
            large_tiles: document.getElementById('large_tiles').value === 'true',
            time_24h: document.getElementById('time_24h').value === 'true',
            gmt_offset: parseInt(document.getElementById('gmt_offset').value),
            web_user: document.getElementById('web_user').value,
            web_pass: document.getElementById('web_pass').value,
            devices: devices
        };

        const btn = document.getElementById('saveBtn');
        const orgHtml = btn.innerHTML;
        btn.innerHTML = `<svg class="animate-spin -ml-1 mr-2 h-5 w-5 text-white" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24"><circle class="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" stroke-width="4"></circle><path class="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path></svg> Saving & Rebooting...`;
        btn.classList.add('opacity-80', 'cursor-not-allowed', 'pointer-events-none');

        fetch('/api/save', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        }).then(res => {
            if (res.ok) {
                waitForPanel("Configuration saved! Rebooting panel...");
            } else if (res.status === 507) {
                saveFailed("Error: Device Memory Full. Delete some devices.");
            } else {
                saveFailed("Error saving configuration.");
            }
        }).catch(() => {
            // The panel answers, waits 300 ms, then reboots — and
            // ESPAsyncWebServer writes the response from another task, so the
            // socket is often cut before the browser has read it. A dropped
            // POST is therefore indistinguishable from a successful one at this
            // level, and calling it an error was wrong more often than right:
            // the settings had usually been written before the reset. Wait and
            // see whether the panel comes back instead of guessing.
            waitForPanel("Panel stopped responding — it usually means it saved and rebooted. Checking...");
        });

        function saveFailed(msg) {
            showToast(msg, true);
            btn.innerHTML = orgHtml;
            btn.classList.remove('opacity-80', 'cursor-not-allowed', 'pointer-events-none');
        }

        // Polls until the panel serves /api/config again, then reloads so the
        // form shows what was actually stored rather than what was typed.
        function waitForPanel(msg) {
            showToast(msg);
            const spin = '<svg class="animate-spin -ml-1 mr-2 h-5 w-5 text-white" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24"><circle class="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" stroke-width="4"></circle><path class="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path></svg>';
            let sec = 0;
            btn.innerHTML = spin + ' Rebooting...';
            const tick = setInterval(() => {
                sec++;
                btn.innerHTML = spin + ' Rebooting... ' + sec + 's';
                if (sec >= 5) {
                    fetch('/api/config', { cache: 'no-store' }).then(r => {
                        if (r.ok) { clearInterval(tick); window.location.reload(); }
                    }).catch(() => {});
                }
                if (sec >= 60) {
                    clearInterval(tick);
                    saveFailed('Panel has not come back. Check its power and Wi-Fi, then refresh this page to see what was saved.');
                }
            }, 1000);
        }
    }
    function updateTimeoutDisplay(val) {
        const secs = parseInt(val);
        document.getElementById('timeout-display').innerText = secs < 60 ? secs + ' sec' : (secs/60) + ' min';
    }

    async function loadWallpaperStatus() {
        try {
            const r = await fetch('/api/wallpaper-status');
            const d = await r.json();
            const el = document.getElementById('wallpaper-status');
            if (d.exists) {
                el.innerHTML = '<span class="text-green-400">&#10003; Wallpaper active</span> &mdash; ' + d.size + ' bytes';
                document.getElementById('current-preview').classList.remove('hidden');
                const wp = document.getElementById('wallpaper-preview');
                await new Promise(r => {
                    wp.onload = r;
                    wp.onerror = r;
                    wp.src = '/wallpaper.jpg?t=' + Date.now();
                });
            } else {
                el.innerText = 'No wallpaper set. Using solid black background.';
                document.getElementById('current-preview').classList.add('hidden');
            }
        } catch(e) { document.getElementById('wallpaper-status').innerText = 'Could not check status.'; }
    }

    async function loadScreensaverTimeout() {
        try {
            const r = await fetch('/api/config');
            const d = await r.json();
            if (d.ss_timeout) {
                const secs = d.ss_timeout;
                document.getElementById('ss_timeout').value = secs;
                updateTimeoutDisplay(secs);
            }
        } catch(e) {}
    }

    function startRestartCountdown(msg) {
        const prog = document.getElementById('upload-progress');
        const bar = document.getElementById('upload-bar');
        const lbl = document.getElementById('upload-label');
        if (prog) prog.classList.remove('hidden');
        if (bar) bar.style.width = '100%';
        showToast(msg + ' Restarting panel...');
        let count = 6;
        const iv = setInterval(() => {
            count--;
            if (lbl) lbl.innerText = 'Restarting panel... reconnecting in ' + count + 's';
            if (count <= 0) {
                clearInterval(iv);
                if (lbl) lbl.innerText = 'Reconnecting...';
                // Poll until panel is back
                const poll = setInterval(() => {
                    fetch('/api/wallpaper-status').then(() => {
                        clearInterval(poll);
                        location.reload();
                    }).catch(() => {});
                }, 1000);
            }
        }, 1000);
    }

    function uploadWallpaper() {
        const file = document.getElementById('wallpaper-file').files[0];
        if (!file) { showToast('Please select an image file first.', true); return; }
        if (file.size > 3145728) { showToast('File too large. Max 3MB.', true); return; }
        const formData = new FormData();
        formData.append('wallpaper', file);
        const prog = document.getElementById('upload-progress');
        const bar = document.getElementById('upload-bar');
        const lbl = document.getElementById('upload-label');
        prog.classList.remove('hidden');
        bar.style.width = '0%';
        lbl.innerText = 'Uploading...';
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/api/wallpaper-upload');
        xhr.upload.onprogress = (e) => {
            if (e.lengthComputable) {
                const pct = Math.round((e.loaded / e.total) * 100);
                bar.style.width = pct + '%';
                lbl.innerText = 'Uploading... ' + pct + '%';
            }
        };
        xhr.onload = () => {
            if (xhr.status === 200) {
                startRestartCountdown('Wallpaper uploaded!');
            } else {
                lbl.innerText = 'Upload failed: ' + xhr.responseText;
                showToast('Upload failed.', true);
            }
        };
        xhr.onerror = () => { showToast('Network error during upload.', true); };
        xhr.send(formData);
    }

    async function deleteWallpaper() {
        if (!confirm('Remove the wallpaper? Panel will restart.')) return;
        document.getElementById('upload-progress').classList.remove('hidden');
        document.getElementById('upload-label').innerText = 'Removing...';
        fetch('/api/wallpaper-delete', { method: 'POST' }).catch(() => {});
        startRestartCountdown('Wallpaper removed.');
    }

    async function applyPreset(id) {
        if (!confirm('Apply this wallpaper? Panel will restart.')) return;
        const prog = document.getElementById('upload-progress');
        const lbl = document.getElementById('upload-label');
        prog.classList.remove('hidden');
        lbl.innerText = 'Applying preset...';
        try {
            const r = await fetch('/api/wallpaper-preset', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({id: id})
            });
            if (r.ok) {
                startRestartCountdown('Preset applied!');
            } else {
                const txt = await r.text().catch(() => '');
                prog.classList.add('hidden');
                showToast('Failed to apply preset: ' + (txt || r.status), true);
            }
        } catch(e) {
            // Network error = panel already restarting
            startRestartCountdown('Preset applied!');
        }
    }

    async function saveScreensaverTimeout() {
        const secs = parseInt(document.getElementById('ss_timeout').value);
        const r = await fetch('/api/ss-timeout', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ss_timeout: secs})
        });
        if (r.ok) showToast('Screensaver timeout saved (' + (secs < 60 ? secs + 's' : (secs/60) + 'min') + '). Applied immediately!');
        else showToast('Failed to save timeout.', true);
    }

    function uploadFirmware() {
        const fileInput = document.getElementById('ota-file');
        const file = fileInput.files[0];
        if (!file) {
            showToast('Please select a .bin firmware file first.', true);
            return;
        }
        if (!file.name.endsWith('.bin')) {
            showToast('Invalid file type. Must be a .bin file.', true);
            return;
        }

        if (!confirm('Warning: Do not power off the device during the firmware update. Ready to flash?')) {
            return;
        }

        const formData = new FormData();
        formData.append('update', file);

        const container = document.getElementById('ota-progress-container');
        const bar = document.getElementById('ota-bar');
        const lbl = document.getElementById('ota-label');
        const pct = document.getElementById('ota-percent');

        container.classList.remove('hidden');
        bar.style.width = '0%';
        pct.innerText = '0%';
        lbl.innerText = 'Uploading firmware...';
        
        // Disable inputs during OTA
        fileInput.disabled = true;
        document.querySelectorAll('button').forEach(b => b.classList.add('pointer-events-none', 'opacity-50'));

        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/api/update');
        
        xhr.upload.onprogress = (e) => {
            if (e.lengthComputable) {
                const percentComplete = Math.round((e.loaded / e.total) * 100);
                bar.style.width = percentComplete + '%';
                pct.innerText = percentComplete + '%';
                
                if (percentComplete === 100) {
                    lbl.innerText = 'Flashing... Do not remove power!';
                    pct.innerText = 'Wait...';
                }
            }
        };

        xhr.onload = () => {
            if (xhr.status === 200) {
                lbl.innerText = 'Update Successful! Rebooting...';
                pct.innerText = '100%';
                bar.classList.replace('from-indigo-500', 'from-emerald-500');
                bar.classList.replace('to-purple-500', 'to-emerald-400');
                
                showToast("Firmware flashed successfully! Panel is restarting...");
                
                // Set interval to poll until device is back online
                setTimeout(() => {
                    lbl.innerText = 'Waiting for device to reconnect...';
                    const poll = setInterval(() => {
                        fetch('/api/wallpaper-status').then(() => {
                            clearInterval(poll);
                            lbl.innerText = 'Device Online! Reloading...';
                            setTimeout(() => location.reload(), 1000);
                        }).catch(() => {});
                    }, 2000);
                }, 5000); // Wait 5s before starting polling

            } else {
                lbl.innerText = 'Update Failed: ' + xhr.responseText;
                bar.classList.replace('from-indigo-500', 'from-red-500');
                bar.classList.replace('to-purple-500', 'to-red-600');
                showToast('Firmware update failed.', true);
                
                // Re-enable UI
                fileInput.disabled = false;
                document.querySelectorAll('button').forEach(b => b.classList.remove('pointer-events-none', 'opacity-50'));
            }
        };

        xhr.onerror = () => {
            lbl.innerText = 'Network connection lost.';
            showToast('Network error during firmware upload.', true);
            fileInput.disabled = false;
            document.querySelectorAll('button').forEach(b => b.classList.remove('pointer-events-none', 'opacity-50'));
        };

        xhr.send(formData);
    }

    // SYSTEM INFO
    // STOCK TICKER CONFIG
    async function loadStockConfig() {
        try {
            const r = await fetch('/api/stock-config');
            if (!r.ok) return;
            const d = await r.json();
            document.getElementById('stock_enabled').checked = d.enabled || false;
            document.getElementById('stock_s0').value = d.symbol0 || '';
            document.getElementById('stock_s1').value = d.symbol1 || '';
            document.getElementById('stock_s2').value = d.symbol2 || '';
            document.getElementById('stock_api_key').value = d.api_key || '';
        } catch (e) { console.error('loadStockConfig', e); }
    }
    async function saveStockConfig() {
        const chk = document.getElementById('stock_enabled');
        const key = document.getElementById('stock_api_key').value.trim();
        const isEnabled = chk ? chk.checked : false;
        console.log('DEBUG: stock_enabled checkbox =', chk, 'checked =', isEnabled);
        const payload = {
            enabled: isEnabled,
            symbol0: document.getElementById('stock_s0').value.trim().toUpperCase(),
            symbol1: document.getElementById('stock_s1').value.trim().toUpperCase(),
            symbol2: document.getElementById('stock_s2').value.trim().toUpperCase(),
            api_key: (key === '********' || key === '') ? null : key
        };
        console.log('DEBUG: payload =', JSON.stringify(payload, null, 2));
        try {
            const r = await fetch('/api/stock-config', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify(payload)
            });
            if (r.ok) showToast('Stock config saved! Takes effect on next screensaver.');
            else showToast('Save failed: ' + (await r.text()), true);
        } catch (e) { showToast('Network error: ' + e.message, true); }
    }
    async function loadSysInfo() {
        try {
            const r = await fetch('/api/sysinfo');
            const d = await r.json();
            const rssi = d.wifi_rssi;
            let rssiLabel = rssi + ' dBm';
            if (rssi >= -50) rssiLabel += ' (Excellent)';
            else if (rssi >= -65) rssiLabel += ' (Good)';
            else if (rssi >= -75) rssiLabel += ' (Fair)';
            else rssiLabel += ' (Weak)';
            document.getElementById('si-rssi').textContent = rssiLabel;

            const sec = d.uptime_sec;
            const h = Math.floor(sec / 3600);
            const m = Math.floor((sec % 3600) / 60);
            document.getElementById('si-uptime').textContent = h + 'h ' + m + 'm';

            document.getElementById('si-heap').textContent = (d.free_heap / 1024).toFixed(1) + ' KB';
            document.getElementById('si-psram').textContent = (d.free_psram / 1024).toFixed(0) + ' KB';

            const pct = ((d.fs_used / d.fs_total) * 100).toFixed(1);
            document.getElementById('si-fs').textContent = (d.fs_used / 1024).toFixed(0) + ' / ' + (d.fs_total / 1024).toFixed(0) + ' KB (' + pct + '%)';

            document.getElementById('si-ip').textContent = location.hostname;
        } catch (e) {
            console.error('sysinfo error', e);
        }
    }

    // BACKUP & RESTORE
    async function downloadBackup() {
        try {
            const [cfgR, scnR, schR] = await Promise.all([
                fetch('/api/config'),
                fetch('/api/scenes'),
                fetch('/api/schedules')
            ]);
            const config = await cfgR.json();
            const scenes_data = await scnR.json();
            const schedules_data = await schR.json();

            const backup = {
                _type: 'sc01_backup',
                _version: 1,
                _date: new Date().toISOString(),
                config: config,
                scenes: scenes_data,
                schedules: schedules_data
            };

            const blob = new Blob([JSON.stringify(backup, null, 2)], {type: 'application/json'});
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = 'sc01_backup_' + new Date().toISOString().slice(0,10) + '.json';
            a.click();
            URL.revokeObjectURL(url);
            showToast('Backup downloaded successfully.');
        } catch (e) {
            showToast('Backup failed: ' + e.message, true);
        }
    }

    async function restoreBackup(input) {
        const file = input.files[0];
        if (!file) return;
        if (!confirm('This will overwrite ALL current settings, devices, scenes, and schedules. Continue?')) {
            input.value = '';
            return;
        }
        try {
            const text = await file.text();
            const backup = JSON.parse(text);
            if (backup._type !== 'sc01_backup') {
                showToast('Invalid backup file format.', true);
                input.value = '';
                return;
            }

            // Restore config (settings + devices)
            if (backup.config) {
                const cfg = backup.config;
                // Remove masked passwords to avoid overwriting with ********
                if (cfg.wifi_pass === '********') delete cfg.wifi_pass;
                if (cfg.mqtt_pwd === '********') delete cfg.mqtt_pwd;
                if (cfg.web_pass === '********') delete cfg.web_pass;
                await fetch('/api/save', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify(cfg)
                });
            }

            // Restore scenes
            if (backup.scenes) {
                await fetch('/api/scenes', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({scenes: backup.scenes})
                });
            }

            // Restore schedules
            if (backup.schedules) {
                await fetch('/api/schedules', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({schedules: backup.schedules})
                });
            }

            showToast('Backup restored successfully! Reloading...');
            setTimeout(() => location.reload(), 2000);
        } catch (e) {
            showToast('Restore failed: ' + e.message, true);
        }
        input.value = '';
    }

    // DOMContentLoaded no longer needed - all loading done in window.onload sequentially

    // SCENE MANAGEMENT
    const MAX_SCENES = 8;
    const MAX_ACTIONS = 10;
    let scenes = [];

    const SCENE_ICONS = [
        { value: 0, name: 'Morning', emoji: '[M]', color: '#FBBF24' },
        { value: 1, name: 'Night',   emoji: '[N]', color: '#6366F1' },
        { value: 2, name: 'Leave',   emoji: '[L]', color: '#22C55E' },
        { value: 3, name: 'Movie',   emoji: '[V]', color: '#EF4444' },
        { value: 4, name: 'Party',   emoji: '[P]', color: '#EC4899' },
        { value: 5, name: 'Custom',  emoji: '[C]', color: '#F59E0B' }
    ];

    async function loadScenes() {
        try {
            const r = await fetch('/api/scenes');
            const d = await r.json();
            scenes = d.scenes || [];
            renderScenes();
        } catch(e) {
            console.error('Failed to load scenes:', e);
            document.getElementById('scenes-list').innerHTML = '<div class="col-span-full text-center py-8 text-slate-500">Failed to load scenes.</div>';
        }
    }

    function renderScenes() {
        const list = document.getElementById('scenes-list');
        list.innerHTML = '';
        document.getElementById('scene-count').innerText = scenes.length + ' / ' + MAX_SCENES;

        if (scenes.length === 0) {
            list.innerHTML = '<div class="col-span-full text-center py-12 text-slate-500 flex flex-col items-center gap-3"><svg xmlns="http://www.w3.org/2000/svg" class="h-12 w-12 text-slate-600" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5" d="M14.752 11.168l-3.197-2.132A1 1 0 0010 9.87v4.263a1 1 0 001.555.832l3.197-2.132a1 1 0 000-1.664z" /><path stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5" d="M21 12a9 9 0 11-18 0 9 9 0 0118 0z" /></svg><span class="text-lg font-medium">No scenes yet</span><span class="text-sm">Click &quot;Add Scene&quot; to create your first automation scene</span></div>';
            return;
        }

        scenes.forEach((sc, si) => {
            const icon = SCENE_ICONS.find(i => i.value === sc.icon) || SCENE_ICONS[5];
            const iconOpts = SCENE_ICONS.map(i => '<option value="' + i.value + '"' + (sc.icon === i.value ? ' selected' : '') + '>' + i.emoji + ' ' + i.name + '</option>').join('');

            let actionsHtml = '';
            const acts = sc.actions || [];
            for (let ai = 0; ai < acts.length; ai++) {
                actionsHtml += renderActionRow(si, ai, acts[ai]);
            }

            list.innerHTML += '' +
                '<div class="bg-cardDark rounded-2xl shadow-lg border border-borderDark overflow-hidden flex flex-col group relative transition-all duration-500 hover:shadow-2xl hover:border-slate-500 ring-1 ring-white/5" style="border-left: 3px solid ' + icon.color + '">' +
                '  <div class="px-5 py-3.5 border-b border-borderDark bg-slate-800/40 flex justify-between items-center">' +
                '    <div class="font-semibold text-slate-200 flex items-center gap-2">' +
                '      <span class="text-xl">' + icon.emoji + '</span>' +
                '      <span class="truncate max-w-[200px]" title="' + (sc.name || '') + '">' + (sc.name || 'Unnamed Scene') + '</span>' +
                '      <span class="text-[10px] text-slate-500 font-normal">' + acts.length + ' action' + (acts.length !== 1 ? 's' : '') + '</span>' +
                '    </div>' +
                '    <button onclick="removeScene(' + si + ')" class="text-slate-500 hover:text-red-400 p-1.5 rounded-lg hover:bg-red-400/10 transition-colors flex items-center gap-1 text-sm" title="Delete Scene">' +
                '      <svg xmlns="http://www.w3.org/2000/svg" class="h-4 w-4" viewBox="0 0 20 20" fill="currentColor"><path fill-rule="evenodd" d="M9 2a1 1 0 00-.894.553L7.382 4H4a1 1 0 000 2v10a2 2 0 002 2h8a2 2 0 002-2V6a1 1 0 100-2h-3.382l-.724-1.447A1 1 0 0011 2H9zM7 8a1 1 0 012 0v6a1 1 0 11-2 0V8zm5-1a1 1 0 00-1 1v6a1 1 0 102 0V8a1 1 0 00-1-1z" clip-rule="evenodd" /></svg> Delete' +
                '    </button>' +
                '  </div>' +
                '  <div class="p-5 space-y-4">' +
                '    <div class="grid grid-cols-2 gap-3">' +
                '      <div>' +
                '        <label class="block text-xs font-semibold text-slate-500 uppercase tracking-wider mb-1.5">Scene Name</label>' +
                '        <input class="w-full bg-slate-900/40 border border-slate-700/80 rounded-lg px-4 py-2.5 text-sm text-slate-200 focus:outline-none focus:ring-2 focus:ring-primary/40 focus:border-primary transition-all shadow-inner" type="text" value="' + (sc.name || '') + '" onchange="scenes[' + si + '].name=this.value; renderScenes()">' +
                '      </div>' +
                '      <div>' +
                '        <label class="block text-xs font-semibold text-slate-500 uppercase tracking-wider mb-1.5">Icon / Type</label>' +
                '        <select class="w-full bg-slate-900/40 border border-slate-700/80 rounded-lg px-4 py-2.5 text-sm text-slate-200 appearance-none focus:outline-none focus:ring-2 focus:ring-primary/40 cursor-pointer shadow-inner" onchange="scenes[' + si + '].icon=parseInt(this.value); scenes[' + si + '].color=SCENE_ICONS.find(i=>i.value===scenes[' + si + '].icon).color; renderScenes()">' + iconOpts + '</select>' +
                '      </div>' +
                '    </div>' +
                '    <div>' +
                '      <label class="block text-xs font-semibold text-slate-500 uppercase tracking-wider mb-2 flex items-center gap-1.5">' +
                '        <svg xmlns="http://www.w3.org/2000/svg" class="h-3.5 w-3.5" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M13 10V3L4 14h7v7l9-11h-7z" /></svg>' +
                '        MQTT Actions' +
                '      </label>' +
                '      <div class="space-y-2" id="scene-actions-' + si + '">' + actionsHtml + '</div>' +
                '      ' + (acts.length < MAX_ACTIONS ? '<button onclick="addAction(' + si + ')" class="mt-2 text-xs text-slate-400 hover:text-primary border border-slate-700 hover:border-primary/50 rounded-lg px-3 py-1.5 transition-all flex items-center gap-1">+ Add Action</button>' : '') +
                '    </div>' +
                '  </div>' +
                '</div>';
        });
    }

    function renderActionRow(si, ai, act) {
        return '' +
            '<div class="flex gap-2 items-center">' +
            '  <input class="flex-[3] bg-slate-900/40 border border-slate-700/80 rounded-lg px-3 py-2 text-[13px] text-slate-300 font-mono focus:outline-none focus:ring-2 focus:ring-primary/40 shadow-inner" type="text" value="' + (act.topic || '') + '" placeholder="cmnd/device/POWER" onchange="scenes[' + si + '].actions[' + ai + '].topic=this.value">' +
            '  <input class="flex-[1.5] bg-slate-900/40 border border-slate-700/80 rounded-lg px-3 py-2 text-[13px] text-slate-300 font-mono focus:outline-none focus:ring-2 focus:ring-primary/40 shadow-inner" type="text" value="' + (act.payload || '') + '" placeholder="ON" onchange="scenes[' + si + '].actions[' + ai + '].payload=this.value">' +
            '  <button onclick="removeAction(' + si + ',' + ai + ')" class="text-slate-600 hover:text-red-400 p-1 rounded transition-colors flex-shrink-0" title="Remove action">' +
            '    <svg xmlns="http://www.w3.org/2000/svg" class="h-4 w-4" viewBox="0 0 20 20" fill="currentColor"><path fill-rule="evenodd" d="M4.293 4.293a1 1 0 011.414 0L10 8.586l4.293-4.293a1 1 0 111.414 1.414L11.414 10l4.293 4.293a1 1 0 01-1.414 1.414L10 11.414l-4.293 4.293a1 1 0 01-1.414-1.414L8.586 10 4.293 5.707a1 1 0 010-1.414z" clip-rule="evenodd" /></svg>' +
            '  </button>' +
            '</div>';
    }

    function addSceneCard() {
        if (scenes.length >= MAX_SCENES) { showToast('Maximum ' + MAX_SCENES + ' scenes reached.', true); return; }
        scenes.push({ name: 'New Scene', icon: 5, color: 0xF59E0B, actions: [{ topic: '', payload: '' }] });
        renderScenes();
        document.getElementById('scenes-list').lastElementChild.scrollIntoView({ behavior: 'smooth' });
    }

    function removeScene(si) {
        if (!confirm('Delete this scene?')) return;
        scenes.splice(si, 1);
        renderScenes();
    }

    function addAction(si) {
        if (!scenes[si].actions) scenes[si].actions = [];
        if (scenes[si].actions.length >= MAX_ACTIONS) return;
        scenes[si].actions.push({ topic: '', payload: '' });
        renderScenes();
    }

    function removeAction(si, ai) {
        scenes[si].actions.splice(ai, 1);
        renderScenes();
    }

    async function saveScenesAPI() {
        // Clean up: remove actions with empty topics
        const cleaned = scenes.map(sc => ({
            name: sc.name || 'Scene',
            icon: sc.icon || 0,
            color: typeof sc.color === 'string' ? parseInt(sc.color.replace('#',''), 16) : (sc.color || 0xF59E0B),
            actions: (sc.actions || []).filter(a => a.topic && a.topic.trim() !== '')
        }));

        try {
            const r = await fetch('/api/scenes', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ scenes: cleaned })
            });
            if (r.ok) {
                showToast('Scenes saved! They will appear on the panel Scenes tab after reboot.');
                loadScenes();
            } else {
                showToast('Failed to save scenes: ' + await r.text(), true);
            }
        } catch(e) {
            showToast('Network error saving scenes.', true);
        }
    }

    // =====================================================
    //  SCHEDULE AUTOMATION
    // =====================================================
    const DAY_LABELS = ['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];
    let scheds = [];

    async function loadSchedules() {
        try {
            const r = await fetch('/api/schedules');
            const data = await r.json();
            scheds = data.schedules || [];
            renderSchedules();
        } catch(e) { console.error('loadSchedules', e); }
    }

    function renderSchedules() {
        const el = document.getElementById('schedules-list');
        document.getElementById('schedule-count').textContent = scheds.length;
        if (scheds.length === 0) {
            el.innerHTML = '<div class="col-span-full text-center py-8 text-slate-500">No schedules yet. Click "Add Schedule" to create one.</div>';
            return;
        }
        el.innerHTML = scheds.map((sc, i) => {
            const scName = (scenes[sc.scene] && scenes[sc.scene].name) ? scenes[sc.scene].name : ('Scene #' + sc.scene);
            const hh = String(sc.hour).padStart(2,'0');
            const mm = String(sc.minute).padStart(2,'0');
            const dayBtns = DAY_LABELS.map((d,di) => {
                const active = sc.days & (1 << di);
                return '<button type="button" onclick="toggleSchedDay('+i+','+di+')" class="w-9 h-9 rounded-lg text-xs font-bold transition-all '+(active ? 'bg-teal-500 text-white shadow-lg shadow-teal-500/30' : 'bg-slate-800 text-slate-500 hover:bg-slate-700')+'">'+d+'</button>';
            }).join('');
            const sceneOpts = scenes.map((s,si) => '<option value="'+si+'"'+(si===sc.scene?' selected':'')+'>'+SCENE_ICONS[s.icon||0].name+' '+s.name+'</option>').join('');
            return '<div class="bg-slate-900/60 border border-slate-700/50 rounded-2xl p-4 relative group">'
                +'<div class="flex items-center justify-between mb-3">'
                +'<div class="flex items-center gap-3">'
                +'<label class="relative inline-flex items-center cursor-pointer">'
                +'<input type="checkbox" '+(sc.enabled?'checked':'')+' onchange="toggleSchedEnabled('+i+')" class="sr-only peer">'
                +'<div class="w-10 h-5 rounded-full transition-colors '+(sc.enabled?'bg-teal-500':'bg-slate-700')+'"><div class="absolute top-0.5 left-[2px] bg-white rounded-full h-4 w-4 transition-transform '+(sc.enabled?'translate-x-5':'')+'"></div></div>'
                +'</label>'
                +'<span class="text-2xl font-mono text-slate-100">'+hh+':'+mm+'</span>'
                +'</div>'
                +'<button onclick="removeSchedule('+i+')" class="text-slate-500 hover:text-red-400 transition-colors p-1 rounded-lg hover:bg-red-500/10">'
                +'<svg class="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="m14.74 9-.346 9m-4.788 0L9.26 9m9.968-3.21c.342.052.682.107 1.022.166m-1.022-.165L18.16 19.673a2.25 2.25 0 0 1-2.244 2.077H8.084a2.25 2.25 0 0 1-2.244-2.077L4.772 5.79m14.456 0a48.108 48.108 0 0 0-3.478-.397m-12 .562c.34-.059.68-.114 1.022-.165m0 0a48.11 48.11 0 0 1 3.478-.397m7.5 0v-.916c0-1.18-.91-2.164-2.09-2.201a51.964 51.964 0 0 0-3.32 0c-1.18.037-2.09 1.022-2.09 2.201v.916m7.5 0a48.667 48.667 0 0 0-7.5 0"/></svg>'
                +'</button>'
                +'</div>'
                +'<div class="mb-3"><select onchange="scheds['+i+'].scene=parseInt(this.value);renderSchedules()" class="w-full bg-slate-800 border border-slate-600 rounded-lg px-3 py-2 text-sm text-slate-200">'+sceneOpts+'</select></div>'
                +'<div class="flex items-center gap-2 mb-3">'
                +'<input type="time" value="'+hh+':'+mm+'" onchange="updateSchedTime('+i+',this.value)" class="bg-slate-800 border border-slate-600 rounded-lg px-3 py-2 text-sm text-slate-200 flex-1">'
                +'<button onclick="setSchedDays('+i+',127)" class="text-[10px] bg-slate-800 hover:bg-slate-700 text-slate-400 px-2 py-1 rounded-lg border border-slate-600">All</button>'
                +'<button onclick="setSchedDays('+i+',62)" class="text-[10px] bg-slate-800 hover:bg-slate-700 text-slate-400 px-2 py-1 rounded-lg border border-slate-600">Weekdays</button>'
                +'<button onclick="setSchedDays('+i+',65)" class="text-[10px] bg-slate-800 hover:bg-slate-700 text-slate-400 px-2 py-1 rounded-lg border border-slate-600">Weekend</button>'
                +'</div>'
                +'<div class="flex gap-1.5 justify-center">'+dayBtns+'</div>'
                +'</div>';
        }).join('');
    }

    function addScheduleCard() {
        if (scheds.length >= 16) { showToast('Max 16 schedules', true); return; }
        if (scenes.length === 0) { showToast('Create a scene first', true); return; }
        scheds.push({ scene: 0, hour: 7, minute: 0, days: 127, enabled: true });
        renderSchedules();
    }

    function removeSchedule(i) { scheds.splice(i, 1); renderSchedules(); }

    function toggleSchedEnabled(i) { scheds[i].enabled = !scheds[i].enabled; renderSchedules(); }

    function toggleSchedDay(i, d) { scheds[i].days ^= (1 << d); renderSchedules(); }

    function setSchedDays(i, mask) { scheds[i].days = mask; renderSchedules(); }

    function updateSchedTime(i, val) {
        const parts = val.split(':');
        scheds[i].hour = parseInt(parts[0]) || 0;
        scheds[i].minute = parseInt(parts[1]) || 0;
        renderSchedules();
    }

    async function saveSchedulesAPI() {
        try {
            const r = await fetch('/api/schedules', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ schedules: scheds })
            });
            if (r.ok) {
                showToast('Schedules saved! Active immediately.');
                loadSchedules();
            } else {
                showToast('Failed to save schedules: ' + await r.text(), true);
            }
        } catch(e) {
            showToast('Network error saving schedules.', true);
        }
    }

    // i18n: Web Portal Translation
    (function(){
      const WEB_I18N = {
        'Hero Home Panel': 'web_title',
        'Save & Restart': 'web_save_restart',
        'Network Setup': 'web_network',
        'Device Management': 'web_device_mgmt',
        'Scenes': 'web_scenes_tab',
        'Schedules': 'web_schedules_tab',
        'Wallpaper': 'web_wallpaper',
        'System & Updates': 'web_system',
        'Network & MQTT Configuration': 'web_mqtt_config',
        'Wi-Fi Credentials': 'web_wifi_cred',
        'MQTT Server IP': 'web_mqtt_server',
        'Weather City': 'web_weather_city',
        'Panel Title': 'web_panel_title',
        'Timezone': 'web_timezone',
        'Web Portal Authentication': 'web_auth',
        'Wallpaper & Screensaver': 'web_wallpaper',
        'Screensaver Timer': 'web_ss_timer',
        'Hardware Devices': 'web_devices',
        'Add Device': 'web_add_device',
        'Scene Automation': 'web_scenes_tab',
        'Add Scene': 'web_add_scene',
        'Save Scenes': 'web_save_scenes',
        'Schedule Automation': 'web_schedules_tab',
        'System & Firmware Updates': 'web_system',
        'IP Address': 'web_ip_address',
        'Backup & Restore': 'web_export_help',
        'Stock Ticker': 'web_stock_ticker',
        'Stock': 'web_stock_ticker',
        'Stock Ticker (Screensaver)': 'web_stock_ticker_title',
        'Enable Stock Ticker': 'web_enable_stock',
        'Shows a compact price bar above the clock on the Flip Clock screensaver': 'web_stock_sub',
        'Asset 1': 'web_symbol_1',
        'Asset 2': 'web_symbol_2',
        'Asset 3': 'web_symbol_3',
        'Twelve Data API Key': 'web_api_key',
        'Save & Apply': 'web_save_apply',
      };
      fetch('/api/lang').then(r=>r.json()).then(dict=>{
        if(!dict || dict._lang==='en') return;
        const walk = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT);
        while(walk.nextNode()){
          let n = walk.currentNode;
          let t = n.textContent.trim();
          let key = WEB_I18N[t];
          if(key && dict[key]){
            n.textContent = n.textContent.replace(t, dict[key]);
          }
        }
        // Translate placeholders
        document.querySelectorAll('[placeholder]').forEach(el=>{
          let pt = el.placeholder.trim();
          let k = WEB_I18N[pt];
          if(k && dict[k]) el.placeholder = dict[k];
        });
        // Translate HTML-content elements (description paragraph with links/code)
        const stockDesc = document.getElementById('stock-desc-p');
        if(stockDesc && dict.web_stock_desc) stockDesc.innerHTML = dict.web_stock_desc;
      }).catch(()=>{});
    })();

</script>
</body>
</html>
)rawliteral";
