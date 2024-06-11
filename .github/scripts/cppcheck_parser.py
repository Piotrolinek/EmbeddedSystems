import xml.etree.ElementTree as ET
import sys


def parse_cppcheck_results(xml_file):
    tree = ET.parse(xml_file)
    root = tree.getroot()

    error_found = False
    for error in root.iter("error"):
        error_id = error.get("id")
        severity = error.get("severity")
        msg = error.get("msg")

        location = error.find("location")
        if location is None:
            continue

        file = location.get("file")
        line = location.get("line")
        column = location.get("column")

        print(
            f"::error:: {file}:{line}:{column}\n",
            f"\tID: {error_id} | Severity: {severity}\n",
            f"\t{msg}",
        )
        error_found = True

    if error_found:
        sys.exit(1)


if len(sys.argv) != 2:
    print("Usage: python parse_xml.py <xml_file>")
    sys.exit(1)

xml_file = sys.argv[1]
parse_cppcheck_results(xml_file)
