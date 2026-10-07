import random

length = 2000
ops = ["+", "-", "*", "/"]

with open(f"cf{length}.mit", "w") as f:
    for i in range(length):
        line = f"x{i} = 5 "
        for j in range(3):
            line += f" {random.choice(ops)} {random.randint(3, 20)}"
        line += ";\n"
        f.write(line)
    combo = f"x{length} = x0"
    for i in range(1, length):
        combo += f" + x{i}"
    combo += ";"
    f.write(combo)
