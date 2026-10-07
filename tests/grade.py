#!/usr/bin/env python3

import subprocess
import tempfile
from pathlib import Path

def run_command(cmd: list[str], timeout: int = 30):
    """Run a command and return the result."""
    try:
        return subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
    except subprocess.TimeoutExpired:
        print(f"Command timed out after {timeout} seconds")
        return None
    except Exception as e:
        print(f"Command failed: {e}")
        return None

def run_lexer_test(test_file: Path) -> tuple[bool, str]:
    """Run a single lexer test and return (passed, message)."""
    expected_lex_file = test_file.with_suffix('.mit.lex')

    with tempfile.NamedTemporaryFile(mode='w', delete=False) as temp_file:
        command = ["./run.sh", "scan", str(test_file), "-o", temp_file.name]
        temp_file_path = temp_file.name
        result = run_command(command)

    if result is None:
        return False, "Test execution failed"

    try:
        if expected_lex_file.exists():
            try:
                with open(expected_lex_file, 'r') as expected, open(temp_file_path, 'r') as actual:
                    expected_content = expected.read()
                    actual_content = actual.read()

                passed = (expected_content.strip() == actual_content.strip())
                message = "Passed" if passed else "Failed - output doesn't match expected"

            finally:
                import os
                os.unlink(temp_file_path)

        else:
            passed = result.returncode != 0
            message = "Passed" if passed else f"Failed - expected error but got exit code {result.returncode}"

        return passed, message

    except Exception as e:
        return False, f"Test execution failed: {str(e)}"

def run_parser_test(test_file: Path) -> tuple[bool, str]:
    """Run a single parser test and return (passed, message)."""
    test_name = test_file.name

    command = ["./run.sh", "parse", str(test_file)]
    result = run_command(command)

    if result is None:
        return False, "Test execution failed"

    if "good" in test_name:
        passed = result.returncode == 0
        expected_behavior = "success"
    elif "bad" in test_name:
        passed = result.returncode != 0
        expected_behavior = "error"
    else:
        passed = False
        expected_behavior = "unknown"

    if passed:
        message = "Passed"
    else:
        message = f"Failed - expected {expected_behavior} but got exit code {result.returncode}"

    return passed, message

def main():
    """Run all phase 1 tests."""
    test_dir = Path("tests/phase1")

    if not test_dir.exists():
        print(f"Test directory {test_dir} not found")
        return

    public_tests = list((test_dir / "public").glob("*.mit"))
    private_tests = list((test_dir / "private").glob("*.mit"))
    all_tests = public_tests + private_tests

    if not all_tests:
        print("No test files found")
        return

    print(f"Running {len(all_tests)} tests ({len(public_tests)} public, {len(private_tests)} private)")
    print()

    passed_tests = 0
    total_tests = len(all_tests) * 2  # Each test runs both lexer and parser

    for test_file in all_tests:
        print(f"Testing {test_file.name}:")

        # Run parser test
        parser_passed, parser_msg = run_parser_test(test_file)
        print(f"  Parser: {parser_msg}")
        if parser_passed:
            passed_tests += 1

        # Run lexer test
        lexer_passed, lexer_msg = run_lexer_test(test_file)
        print(f"  Lexer: {lexer_msg}")
        if lexer_passed:
            passed_tests += 1

        print()

    print(f"Results: {passed_tests}/{total_tests} tests passed")

    if passed_tests == total_tests:
        print("All tests passed!")
    else:
        print("Some tests failed.")

if __name__ == "__main__":
    main()
