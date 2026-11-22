"""Fail if the settings in device-builder.yaml drift from wmbus.yaml."""
import pathlib, sys

root = pathlib.Path(__file__).resolve().parent.parent


def settings(name):
    text = (root / name).read_text()
    return text.split("substitutions:\n", 1)[1].split("packages:\n", 1)[0]


if settings("wmbus.yaml") != settings("device-builder.yaml"):
    sys.exit("device-builder.yaml: the substitutions differ from wmbus.yaml")
print("device-builder.yaml settings match wmbus.yaml")
