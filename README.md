Build the project
======================
1. Install esp-idf 5.1.2 https://docs.espressif.com/projects/esp-idf/en/v5.1.2/esp32/get-started/index.html
2. Intsall Git
3. Clone the project:
```
git clone --recurse-submodules https://github.com/szevczenko/esp32_valves_regulator.git
```
4. Init submodules for hq_components:
```
git submodule update --init --recursive
```
5. Run ESP-IDF 5.1 CMD and enter to cloned folder.
6. build by running command:
```
idf.py build
```
7. flash esp32:
```
idf.py flash -p COM8
```
