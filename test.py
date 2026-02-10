import subprocess
import platform
import os
import tempfile

passed = True

class Test:
    def __init__(self, name, code, expected_output):
        self.name = name
        self.code = code
        self.expected_output = expected_output
        self.actual_output = ""

def run(test_name, code, expected_output, no_add=False):
    global passed

    print(f"[?] Testing {test_name}...", end="\r")

    test = Test(test_name, code, expected_output)

    # Wrap code in print() if no_add is False
    if not no_add:
        code = f"print({code});"

    # Determine paths based on platform
    if platform.system() == "Windows":
        compiler = "..\\build\\minic.exe"
        runner = "..\\build\\minir.exe"
        temp_dir = os.environ.get('TEMP', tempfile.gettempdir())
    else:
        compiler = "../build/minic"
        runner = "../build/minir"
        temp_dir = "/dev/shm" if os.path.exists("/dev/shm") else tempfile.gettempdir()

    # Create unique temp file names
    import time
    timestamp = str(int(time.time() * 1000000))
    temp_src = os.path.join(temp_dir, f"minis_test_{timestamp}.mi")
    temp_bc = os.path.join(temp_dir, f"test_{timestamp}.vbc")

    try:
        # Write source code to temp file
        with open(temp_src, 'w', encoding='utf-8') as f:
            f.write(code)

        # Compile the code
        compile_result = subprocess.run(
            [compiler, temp_src, "-o", temp_bc],
            capture_output=True,
            text=True
        )

        if compile_result.returncode != 0:
            print(f"[✖]")
            passed = False
            return test

        # Run the bytecode
        run_result = subprocess.run(
            [runner, temp_bc],
            capture_output=True,
            text=True
        )

        test.actual_output = run_result.stdout.strip()

        # Check if output matches expected
        if test.actual_output == expected_output:
            print(f"[✔]")
        else:
            print(f"[✖]")
            passed = False

    finally:
        # Clean up temp files immediately
        try:
            if os.path.exists(temp_src):
                os.remove(temp_src)
            if os.path.exists(temp_bc):
                os.remove(temp_bc)
        except:
            pass

    return test

# Example tests

run("printing", 'Hello', "Hello")
run("single add", '2 + 2', "4")
run("multi add", '1 + 1 + 1', "3")

circuit = """
for i as 1..6 {
  str x = {
    (i == 1) -> "one!";
    (i == 2) -> "two!";
    (i == 3) -> "three!";
    (i == 4) -> "four!";
    (i == 5) -> "five";
    _ -> "aww :(";
  }
  print(x);
}
"""
run("circuit variables", circuit, "one!two!three!four!five!aww :(", True)
del circuit

run("increment", '1++;', "2")
run("neg", "neg(34)", "-34")
run("abs", "abs(-34)", "34")
run("max", "max([1,3])", "3")
run("min", "min([1,3])", "1")
run("sort", "sort([1,5,3,2,4])", "[1,2,3,4,5]")
run("reverse", "reverse([1,2])", "[2,1]")
run("len", "len([0,0,0])", "3")

print('PASS' if passed else 'FAIL')
