Procedure to detect linear superpolies in the polynomial function that generates the first keystream bit of the RCR-32.
----------------------------------------------------------------------------------------------------------------------
1. Download the C and Python files to a directory.
2. Open a linux terminal and go to the directory where the C and Python files are located.
3. Run the following command: **gcc -o rcr32_linux.so -shared -fPIC -O2 test_rcr32.c**
4. Install the Python3 modules NumPy and SymPy.
5. Run the following command to search for the linear superpolies: **python3 cube_attack_rcr.py**.
6. Run the following command to verify the cube indices detected (list of detected cube indices is given by the variable _**ind**_ in the code cube_attack_rcr_verify.py): **python3 cube_attack_rcr_verify.py**.
