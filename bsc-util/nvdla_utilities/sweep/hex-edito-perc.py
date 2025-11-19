import numpy as np
import sys


def softmax(scores):
    """
    Computes the softmax probabilities for a list of scores.
    Uses a stabilization trick (subtracting the max) to prevent overflow.
    """
    # Ensure scores are a numpy array
    scores = np.array(scores)

    # Stabilization: subtract the max score from all scores
    # This doesn't change the final probabilities but prevents
    # e^x from becoming infinitely large (overflow).
    stable_scores = scores - np.max(scores)

    # Exponentiate the stabilized scores
    exp_scores = np.exp(stable_scores)

    # Divide each exponentiated score by the sum of all
    # to get the final probabilities (percentages).
    probabilities = exp_scores / np.sum(exp_scores)

    return probabilities


def process_hex_string(hex_string):
    """
    Processes a 40-character hex string, converts it to 10
    Little-Endian float16 scores, and prints the softmax probabilities.
    """
    # 1. --- Validation ---
    # Clean up any spaces and ensure it's a 40-char string (20 bytes)
    hex_string = hex_string.replace(" ", "").strip()
    if len(hex_string) != 40:
        print(
            f"Error: Hex string must be 40 characters long, but was {len(hex_string)}.",
            file=sys.stderr,
        )
        return

    try:
        # 2. --- Hex to Bytes ---
        # Convert the entire hex string into a sequence of bytes
        byte_data = bytes.fromhex(hex_string)
        if len(byte_data) != 20:
            raise ValueError("Hex string did not convert to 20 bytes.")

        # 3. --- Bytes to float16 Scores ---
        # Use numpy to interpret the 20 bytes as 10 (20 / 2)
        # float16 numbers. On most machines, this defaults
        # to Little-Endian, which is what we want.
        scores = np.frombuffer(byte_data, dtype=np.float16)

        if scores.size != 10:
            print(
                f"Error: Expected 10 scores, but got {scores.size}.",
                file=sys.stderr,
            )
            return

        # 4. --- Calculate Softmax ---
        probabilities = softmax(scores)

        # 5. --- Print Formatted Results ---
        print(f"--- Results for: {hex_string[:10]}... ---")
        print("=" * 40)
        print("| Class  |  Score  |  Percentage  |")
        print("|---------------|-----------------|--------------|")

        for i in range(10):
            score_str = f"{scores[i]:>15.5f}"
            prob_str = f"{probabilities[i] * 100:>12.2f}%"
            print(f"| {i:<13} | {score_str} | {prob_str} |")

        print("=" * 40)

        winner_class = np.argmax(probabilities)
        winner_prob = np.max(probabilities) * 100
        print(
            f"\nConclusion: Top guess is Class {winner_class} with {winner_prob:.2f}% probability.\n"
        )

    except ValueError as e:
        print(f"Error processing hex string: {e}", file=sys.stderr)
        print(
            "Please ensure the string contains only valid hex characters (0-9, a-f)."
        )
    except Exception as e:
        print(f"An unexpected error occurred: {e}", file=sys.stderr)


# --- Main execution ---
if __name__ == "__main__":

    # Example 1: The one that was 97.18% Class 8
    hex_1 = "E0C601BE35414C4788BFB8BAE7C57CBC8F48CFC4"

    # Example 10%:
    hex_2 = "D8C6AFBD2D413C4732BFACBAE6C585BC8448D1C4"

    # Example 40%
    hex_3 = "EEC634BE54415047B9BFF3BAE8C589BC9748BEC4"

    # Example 80%
    hex_4 = "D5C6BBBD334148476CBF01BBFCC557BC8448B5C4"

    hex_5 = "D5C6E5BD35413347ACBFFABAE1C576BC8E48DAC4"
    # good test this one
    hex_6 = "DFC60CBE2E41514789BFBEBAEAC580BC8E48C8C4"

    hex_7 = "DFC6E8BD31415047C3BFAABAE6C5AEBC9148BFC4"
    hex_8 = "DFC6FABD404149478CBFC5BAE4C581BC9048DBC4"

    # from here is a different picture image2824.pgm
    hex_9 = "1B4CCEC612408DC6D4C4C8C288B84442E3BFB8B9"

    hex_10 = "1C4CCEC60D4091C6D0C4CDC28CB84842DFBFB2B9"
    hex_11 = "1D4CD5C6F23F85C6DBC4BCC23AB82F42BBBFC9B9"

    hex_12 = "214CCCC61A408FC6DBC4F6C24EB81D42C4BFE7B9"

    # from here with new logic stuckbit, for second image
    hex_13 = "144CD66712415FC67AC589C216AEDB41E4BF10BD"

    # for first image
    hex_14 = "51DCFF7B225FD65403E0A9617EDAE25DA8E2FBDC"

    # image from 5 repo vannila logic
    hex_15 = "72C627C48DC76F3F05C8524C613715C53B441745"
    # image from 5 repo with logic
    hex_16 = "46F4E15B7F754C7515F7E778E4F4676DEEF75CF5"

    # image 5/1235
    hex_17 = "2FC60AC2E8C83743A2C8604C5EB542C6B2448344"

    # image 5/7742
    hex_18 = "73C93ABAEAC963440FC8324EB14420CA0D48F2B8"

    # image 3/8518
    hex_19 = "18C89CC20F3D6A4E31C91B3DA1CA28C03BBB733C"

    # image 3/2312
    hex_20 = "29C752C682C0124C50C3F745BDC6A1C36442CE38"

    # image 7/2301
    hex_21 = "8DC1BD40CD423E4371C665C5BAC9194B19B7CF31"
    # image 7/2301
    hex_22 = "6CDBFF7B0461E4D4A3DC235D92DB1F5E28E07FDF"
    # image 7/2301
    hex_23 = "0000000000000000000000000000000000000000"
    # image 7/2301
    hex_24 = "81FC81FC81FC81FC81FC81FC81FC81FC81FC81FC"

    print("--- Running Test Vannila ---")
    process_hex_string(hex_1)

    print("\n--- Running Test 10% ---")
    process_hex_string(hex_2)

    print("\n--- Running Test 40% ---")
    process_hex_string(hex_3)

    print("\n--- Running Test 80% ---")
    process_hex_string(hex_4)

    print("\n--- Running Test 3*10^-3% ---")
    process_hex_string(hex_5)

    print("\n--- Running Test 2*10^-4% ---")
    process_hex_string(hex_6)

    print("\n--- Running Test 8*10^-4% ---")
    process_hex_string(hex_7)

    print("\n--- Running Test 5*10^-5% ---")
    process_hex_string(hex_8)

    print("\n--- Running Test Different Picture Vanilla ---")
    process_hex_string(hex_9)

    print("\n--- Running Test 5*10^-5% ---")
    process_hex_string(hex_10)

    print("\n--- Running Test 8*10^-4% ---")
    process_hex_string(hex_11)

    print("\n--- Running Test 2*10^-3% ---")
    process_hex_string(hex_12)

    print("\n--- Running Test 5*10^-4% ---")
    process_hex_string(hex_13)

    print("\n--- Running Test 5*10^-4% ---")
    process_hex_string(hex_14)

    print("\n--- Running Test 5*10^-4% ---")
    process_hex_string(hex_15)

    print("\n--- Running Test 5*10^-4% ---")
    process_hex_string(hex_16)

    print("\n--- Running Test VANILLA ---")
    process_hex_string(hex_17)

    print("\n--- Running Test WVANILLA ---")
    process_hex_string(hex_18)

    print("\n--- Running Test VANILLA ---")
    process_hex_string(hex_19)

    print("\n--- Running Test VANILLA ---")
    process_hex_string(hex_20)

    print("\n--- Running Test VANILLA ---")
    process_hex_string(hex_21)

    print("\n--- Running Test with pfail 5*10^-5---")
    process_hex_string(hex_22)

    print("\n--- Running Test with pfail 5*10^-5 without stuck bit ---")
    process_hex_string(hex_23)

    print("\n--- Running Test with pfail 5*10^-2 with stuck bit ---")
    process_hex_string(hex_24)
