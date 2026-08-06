#pragma once
// web_page.h
// Declares the single-page web portal payload served at GET /.
// The markup itself lives in web_page.cpp so web_server.cpp stays readable.

// Defined with PROGMEM in web_page.cpp. Declared extern here (without the
// attribute) so the definition gets external linkage -- a namespace-scope
// `const` is internal-linkage by default in C++.
extern const char index_html[];
