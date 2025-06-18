#!/usr/bin/env python3

import os
import re
import argparse

# Regular expression to match SYS_PORT_TRACING_* macros
TRACING_MACRO_REGEX = r"SYS_PORT_TRACING_(OBJ_FUNC|FUNC|OBJ_INIT|FUNC_ENTER|FUNC_EXIT|OBJ_FUNC_ENTER|OBJ_FUNC_EXIT|OBJ_FUNC_BLOCKING)\(([^)]+)\)"
def parse_file(file_path):
    """
    Parse a file and extract SYS_PORT_TRACING_* macro calls, including multi-line macros.

    Args:
        file_path (str): Path to the file to parse.

    Returns:
        list: List of tuples containing macro type and arguments.
    """
    macros = []
    try:
        with open(file_path, "r") as f:
            content = f.read()
            # Remove line continuations for macros split across lines
            content = re.sub(r"\\\n", "", content)
            # Find all macro matches in the file content
            matches = re.findall(TRACING_MACRO_REGEX, content)
            for macro_type, macro_args in matches:
                macros.append((macro_type, macro_args.strip()))
    except Exception as e:
        print(f"Error reading file {file_path}: {e}")
    return macros

def generate_systemview_hooks(macros):
    """
    Generate SEGGER SystemView hooks for the given macros.

    Args:
        macros (list): List of tuples containing macro type and arguments.

    Returns:
        str: Generated SystemView hook code.
    """
    print(f"macros: {macros}")
    hooks = set()  # Use a set to avoid duplicates
    for macro_type, macro_args in macros:
        args = [arg.strip() for arg in macro_args.split(",")]
        if macro_type == "OBJ_FUNC_ENTER":
            # Generate hook for OBJ_FUNC_ENTER
            obj_type, func_name, *params = args
            tid_name = f"TID_{obj_type.upper()}_{func_name.upper()}"
            param_list = ", ".join(params)
            if len(params) == 1:
                hook = f"#define sys_port_trace_{obj_type}_{func_name}_enter({param_list}) \\\n" \
                       f"    SEGGER_SYSVIEW_RecordU32({tid_name}, (uint32_t)(uintptr_t){params[0]})"
            elif len(params) == 2:
                hook = f"#define sys_port_trace_{obj_type}_{func_name}_enter({param_list}) \\\n" \
                       f"    SEGGER_SYSVIEW_RecordU32x2({tid_name}, (uint32_t)(uintptr_t){params[0]}, (uint32_t)(uintptr_t){params[1]})"
            elif len(params) == 3:
                hook = f"#define sys_port_trace_{obj_type}_{func_name}_enter({param_list}) \\\n" \
                       f"    SEGGER_SYSVIEW_RecordEndCallU32x3({tid_name}, (uint32_t)(uintptr_t){params[0]}, (uint32_t)(uintptr_t){params[1]}, (uint32_t)(uintptr_t){params[2]})"
            elif len(params) == 4:
                hook = f"#define sys_port_trace_{obj_type}_{func_name}_enter({param_list}) \\\n" \
                       f"    SEGGER_SYSVIEW_RecordEndCallU32x4({tid_name}, (uint32_t)(uintptr_t){params[0]}, (uint32_t)(uintptr_t){params[1]}, (uint32_t)(uintptr_t){params[2]}, (uint32_t)(uintptr_t){params[3]})"
            else:
                hook = f"#define sys_port_trace_{obj_type}_{func_name}_enter({param_list}) \\\n" \
                       f"    SEGGER_SYSVIEW_RecordU32({tid_name}, (uint32_t)(uintptr_t){params[0]})"
            hooks.add(hook)
        elif macro_type == "OBJ_FUNC_EXIT":
            # Generate hook for OBJ_FUNC_EXIT
            obj_type, func_name, *params = args
            tid_name = f"TID_{obj_type.upper()}_{func_name.upper()}"
            param_list = ", ".join(params)
            if len(params) == 1:
                hook = f"#define sys_port_trace_{obj_type}_{func_name}_exit({param_list}) \\\n" \
                       f"    SEGGER_SYSVIEW_RecordEndCallU32({tid_name}, (uint32_t)({params[0]}))"
            elif len(params) == 2:
                hook = f"#define sys_port_trace_{obj_type}_{func_name}_exit({param_list}) \\\n" \
                       f"    SEGGER_SYSVIEW_RecordEndCallU32x2({tid_name}, (uint32_t)(uintptr_t){params[0]}, (uint32_t)(uintptr_t){params[1]})"
            elif len(params) == 3:
                hook = f"#define sys_port_trace_{obj_type}_{func_name}_exit({param_list}) \\\n" \
                       f"    SEGGER_SYSVIEW_RecordEndCallU32x3({tid_name}, (uint32_t)(uintptr_t){params[0]}, (uint32_t)(uintptr_t){params[1]}, (uint32_t)(uintptr_t){params[2]})"
            else:
                hook = f"#define sys_port_trace_{obj_type}_{func_name}_exit({param_list}) \\\n" \
                       f"    SEGGER_SYSVIEW_RecordEndCallU32({tid_name}, (uint32_t)({params[-1]}))"
            hooks.add(hook)
        elif macro_type == "OBJ_FUNC_BLOCKING":
            # Generate hook for OBJ_FUNC_BLOCKING
            obj_type, func_name, *params = args
            tid_name = f"TID_{obj_type.upper()}_{func_name.upper()}"
            param_list = ", ".join(params)
            if len(params) == 3:
                hook = f"#define sys_port_trace_{obj_type}_{func_name}_blocking({param_list}) \\\n" \
                       f"    SEGGER_SYSVIEW_RecordU32x3({tid_name}, (uint32_t)(uintptr_t){params[0]}, (uint32_t)(uintptr_t){params[1]}, (uint32_t)(uintptr_t){params[2]})"
            else:
                hook = f"#define sys_port_trace_{obj_type}_{func_name}_blocking({param_list}) \\\n" \
                       f"    SEGGER_SYSVIEW_RecordU32({tid_name}, (uint32_t)(uintptr_t){params[0]})"
            hooks.add(hook)
        elif macro_type == "FUNC_ENTER":
            # Generate hook for FUNC_ENTER
            obj_type, func_name, *params = args
            tid_name = f"TID_{obj_type.upper()}_{func_name.upper()}"
            param_list = ", ".join(params)
            if len(params) == 1:
                hook = f"#define sys_port_trace_{obj_type}_{func_name}_enter({param_list}) \\\n" \
                       f"    SEGGER_SYSVIEW_RecordU32({tid_name}, (uint32_t)(uintptr_t){params[0]})"
            elif len(params) == 2:
                hook = f"#define sys_port_trace_{obj_type}_{func_name}_enter({param_list}) \\\n" \
                       f"    SEGGER_SYSVIEW_RecordU32x2({tid_name}, (uint32_t)(uintptr_t){params[0]}, (uint32_t)(uintptr_t){params[1]})"
            else:
                hook = f"#define sys_port_trace_{obj_type}_{func_name}_enter({param_list}) \\\n" \
                       f"    SEGGER_SYSVIEW_RecordU32({tid_name}, (uint32_t)(uintptr_t){params[0]})"
            hooks.add(hook)
        elif macro_type == "FUNC_EXIT":
            # Generate hook for FUNC_EXIT
            obj_type, func_name, *params = args
            tid_name = f"TID_{obj_type.upper()}_{func_name.upper()}"
            hook = f"#define sys_port_trace_{obj_type}_{func_name}_exit(ret) \\\n" \
                   f"    SEGGER_SYSVIEW_RecordEndCallU32({tid_name}, (uint32_t)(ret))"
            hooks.add(hook)
        elif macro_type == "OBJ_INIT":
            # Generate hook for OBJ_INIT
            obj_type = args[0]
            tid_name = f"TID_{obj_type.upper()}_INIT"
            hook = f"#define sys_port_trace_{obj_type}_init({obj_type}) \\\n" \
                   f"    SEGGER_SYSVIEW_RecordU32({tid_name}, (uint32_t)(uintptr_t){obj_type})"
            hooks.add(hook)
    return "\n\n".join(sorted(hooks))  # Sort hooks for consistent output

def main():
    parser = argparse.ArgumentParser(description="Parse source/header files and generate SystemView hooks for SYS_PORT_TRACING_* macros.")
    parser.add_argument("path", help="Path to the source/header file or directory to parse.")
    args = parser.parse_args()

    if os.path.isfile(args.path):
        files = [args.path]
    elif os.path.isdir(args.path):
        files = [os.path.join(root, file) for root, _, files in os.walk(args.path) for file in files if file.endswith((".c", ".h"))]
    else:
        print(f"Invalid path: {args.path}")
        return

    all_macros = []
    for file_path in files:
        macros = parse_file(file_path)
        if macros:
            print(f"Found {len(macros)} macros in {file_path}")
            all_macros.extend(macros)

    if all_macros:
        hooks = generate_systemview_hooks(all_macros)
        print("\nGenerated SystemView hooks:\n")
        print(hooks)
    else:
        print("No SYS_PORT_TRACING_* macros found.")

if __name__ == "__main__":
    main()
