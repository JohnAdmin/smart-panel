// mem_probe.h — temporary heap-leak probe for the panel-stuck investigation.
// Enabled only when the build defines MEM_PROBE=1 (see platformio.ini). Remove
// the flag — and this pair of files — once the investigation closes.
#pragma once
void mem_probe_start();
