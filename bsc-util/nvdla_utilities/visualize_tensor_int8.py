import sys
import matplotlib.pyplot as plt
import numpy as np


def process_tensor():
    map_side = int(sys.argv[2])

    with open(sys.argv[1], "r") as f:
        array = np.array(
            [int(x, 16) for x in f.read().split()], dtype=np.int64
        )
    array = array.reshape(-1, 32)
    result = array.reshape(array.shape[0], -1, 8)

    mp = []
    for i in range(8):
        mp.append(result[:, :, i].reshape(-1, map_side))

    # Convert to numpy as unsigned first
    fig, axes = plt.subplots(2, 4, figsize=(10, 5))  # 8 slices → 2x4 grid

    for i, ax in enumerate(axes.flat):
        arr_uint8 = np.array(mp[i], dtype=np.uint8)
        arr_int8 = arr_uint8.view(np.int8)
        ax.imshow(
            arr_int8, cmap="gray", vmin=-128, vmax=127, interpolation="nearest"
        )
        ax.axis("off")

    plt.tight_layout()
    plt.savefig("tens.png")


def print_help():
    print("tensor visualization script")
    print("Usage:")
    print("python3 visualize_tensor.py <tensor file> <map side length>")


def main():
    if len(sys.argv) < 3:
        print_help()
        quit()
    process_tensor()


if __name__ == "__main__":
    main()
