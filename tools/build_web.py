#!/usr/bin/env python3
"""Puts the WebAssembly build of the game into the web page (one self-contained file).

    python3 tools/build_web.py output/web/petsim.wasm web/index.html output/web/petsim.html
"""
import base64
import sys

wasm, template, out = sys.argv[1:4]
with open(wasm, "rb") as f:
    data = base64.b64encode(f.read()).decode()
with open(template, encoding="utf-8") as f:
    page = f.read()
if page.count("__WASM_BASE64__") != 1:
    sys.exit("the page must contain __WASM_BASE64__ once")
with open(out, "w", encoding="utf-8") as f:
    f.write(page.replace("__WASM_BASE64__", data))
print(f"{out}: {len(page) + len(data)} bytes")
