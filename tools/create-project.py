#!/usr/bin/env python3
"""create-project.py — Scaffold a new Pulp plugin project from templates.

Usage:
    python3 tools/create-project.py "MyPlugin" --type effect --manufacturer "MyCompany"
    python3 tools/create-project.py "MySynth" --type instrument --bundle-id "com.myco.mysynth"
"""

import argparse
import os
import random
import re
import string
import sys
from pathlib import Path


def to_class_name(name: str) -> str:
    """Convert 'My Plugin' to 'MyPlugin'."""
    return re.sub(r'[^a-zA-Z0-9]', '', name.title().replace(' ', ''))


def to_lower_name(name: str) -> str:
    """Convert 'My Plugin' to 'my-plugin'."""
    s = re.sub(r'[^a-zA-Z0-9\s]', '', name)
    return re.sub(r'\s+', '-', s.strip().lower())


def to_namespace(name: str) -> str:
    """Convert 'My Plugin' to 'my_plugin'."""
    s = re.sub(r'[^a-zA-Z0-9\s]', '', name)
    return re.sub(r'\s+', '_', s.strip().lower())


def to_factory_name(name: str) -> str:
    """Convert 'My Plugin' to 'my_plugin'."""
    return to_namespace(name)


def make_plugin_code(name: str) -> str:
    """Generate a 4-char plugin code from the name."""
    clean = re.sub(r'[^a-zA-Z]', '', name)
    if len(clean) >= 4:
        return clean[:4]
    return (clean + 'xxxx')[:4]


def make_manufacturer_code(manufacturer: str) -> str:
    """Generate a 4-char manufacturer code."""
    clean = re.sub(r'[^a-zA-Z]', '', manufacturer)
    if len(clean) >= 4:
        return clean[:4]
    return (clean + 'xxxx')[:4]


def make_vst3_uid() -> str:
    """Generate a random VST3 UID (16 hex bytes)."""
    return ', '.join(f'0x{random.randint(0, 255):02X}' for _ in range(16))


def expand_template(template: str, replacements: dict) -> str:
    """Replace {{KEY}} placeholders in a template."""
    result = template
    for key, value in replacements.items():
        result = result.replace('{{' + key + '}}', value)
    return result


def main():
    parser = argparse.ArgumentParser(description='Scaffold a new Pulp plugin project')
    parser.add_argument('name', help='Plugin name (e.g., "My Gain")')
    parser.add_argument('--type', choices=['effect', 'instrument'], default='effect',
                        help='Plugin type (default: effect)')
    parser.add_argument('--manufacturer', default='Pulp',
                        help='Manufacturer name (default: Pulp)')
    parser.add_argument('--bundle-id', default=None,
                        help='Bundle ID (auto-generated if omitted)')
    parser.add_argument('--version', default='1.0.0',
                        help='Version string (default: 1.0.0)')
    parser.add_argument('--formats', default='VST3 AU CLAP Standalone',
                        help='Space-separated format list (default: VST3 AU CLAP Standalone)')
    parser.add_argument('--output', '-o', default=None,
                        help='Output directory (default: examples/<lower-name>/)')
    parser.add_argument('--description', default='A Pulp audio plugin',
                        help='Plugin description')
    args = parser.parse_args()

    # Compute derived names
    class_name = to_class_name(args.name)
    lower_name = to_lower_name(args.name)
    namespace = to_namespace(args.name)
    factory_name = to_factory_name(args.name)
    plugin_code = make_plugin_code(class_name)
    mfr_code = make_manufacturer_code(args.manufacturer)
    bundle_id = args.bundle_id or f'com.{to_namespace(args.manufacturer)}.{namespace}'
    header_name = f'{lower_name}.hpp'
    target_name = class_name

    # Find project root
    script_dir = Path(__file__).resolve().parent
    root = script_dir.parent
    template_dir = root / 'tools' / 'templates' / args.type

    if not template_dir.exists():
        print(f'Error: template directory not found at {template_dir}', file=sys.stderr)
        return 1

    # Output directory
    if args.output:
        output_dir = Path(args.output)
        if not output_dir.is_absolute():
            output_dir = root / output_dir
    else:
        output_dir = root / 'examples' / lower_name

    if output_dir.exists():
        print(f'Error: {output_dir} already exists', file=sys.stderr)
        return 1

    # Build replacements
    replacements = {
        'PLUGIN_NAME': args.name,
        'CLASS_NAME': class_name,
        'LOWER_NAME': lower_name,
        'NAMESPACE': namespace,
        'FACTORY_NAME': factory_name,
        'HEADER_NAME': header_name,
        'TARGET_NAME': target_name,
        'MANUFACTURER': args.manufacturer,
        'MANUFACTURER_CODE': mfr_code,
        'BUNDLE_ID': bundle_id,
        'VERSION': args.version,
        'PLUGIN_CODE': plugin_code,
        'FORMATS': args.formats,
        'DESCRIPTION': args.description,
        'VST3_UID': make_vst3_uid(),
    }

    # Create output directory
    output_dir.mkdir(parents=True)

    # Process templates
    file_map = {
        'processor.hpp.template': header_name,
        'CMakeLists.txt.template': 'CMakeLists.txt',
        'clap_entry.cpp.template': 'clap_entry.cpp',
        'vst3_entry.cpp.template': 'vst3_entry.cpp',
        'au_v2_entry.cpp.template': 'au_v2_entry.cpp',
        'test.cpp.template': f'test_{lower_name.replace("-", "_")}.cpp',
    }

    for template_file, output_file in file_map.items():
        template_path = template_dir / template_file
        if not template_path.exists():
            continue
        template = template_path.read_text()
        content = expand_template(template, replacements)
        (output_dir / output_file).write_text(content)
        print(f'  Created {output_file}')

    # Add standalone main.cpp
    if 'Standalone' in args.formats:
        main_cpp = f'''#include "{header_name}"
#include <pulp/format/format.hpp>

int main(int argc, char* argv[]) {{
    return pulp::format::run_standalone({namespace}::create_{factory_name}, argc, argv);
}}
'''
        (output_dir / 'main.cpp').write_text(main_cpp)
        print('  Created main.cpp')

    print(f'\nScaffolded "{args.name}" at {output_dir}')
    print(f'\nTo build:')
    try:
        rel = output_dir.relative_to(root)
        print(f'  1. Add add_subdirectory({rel}) to examples/CMakeLists.txt')
    except ValueError:
        print(f'  1. Add the project directory to your CMakeLists.txt')
    print(f'  2. cmake -B build && cmake --build build')
    print(f'  3. ctest --test-dir build -R {lower_name.replace("-", "_")}')

    return 0


if __name__ == '__main__':
    sys.exit(main() or 0)
