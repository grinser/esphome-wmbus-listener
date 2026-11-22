"""Copy the three load-chart lambdas from packages/load_chart.yaml into C++ functions.

Usage: python3 extract.py ../packages/load_chart.yaml lambdas.inc
"""
import re, sys, textwrap

src = open(sys.argv[1]).read().split("\n")


def block_after(anchor_regex, start=0):
    """Body of the first `- lambda: |-` after a line matching anchor_regex."""
    i = next(n for n in range(start, len(src)) if re.search(anchor_regex, src[n]))
    j = next(n for n in range(i, len(src)) if "lambda: |-" in src[n])
    ind = len(src[j + 1]) - len(src[j + 1].lstrip())
    body = []
    for line in src[j + 1:]:
        if line.strip() and len(line) - len(line.lstrip()) < ind:
            break
        body.append(line)
    return textwrap.dedent("\n".join(body))


restore = block_after(r"on_time_sync:")
energy = block_after(r"id: (!extend )?energy_import$")
power = block_after(r"id: (!extend )?power_import$")
with open(sys.argv[2], "w") as f:
    f.write("void restore_fn() {\n" + restore + "\n}\n")
    f.write("void energy_fn(float x) {\n" + energy + "\n}\n")
    f.write("void power_fn(float x) {\n" + power + "\n}\n")
