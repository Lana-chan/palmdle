#!/usr/bin/python3
import sys

def word_to_b26(word):
	i = 0
	for c in word.strip():
		i *= 26
		i += int(ord(c) - ord('a'))
	return i

if __name__=="__main__":
	binary = bytearray()
	count = 0
	index = [0] * 676
	with open("scripts/allowed-guesses.txt", "r") as file:
		for line in file:
			if (line.strip()):
				num = word_to_b26(line[2:5])
				index[word_to_b26(line[0:2])] += 1
				binary.extend(num.to_bytes(2, byteorder='big'))
				count += 1
	answers = ""
	answer_count = 0
	with open("scripts/answer-list.txt", "r") as file:
		for line in file:
			if (line.strip()):
				answers += line.strip().upper()
				answer_count += 1
	with open("Src/wordlists.h", "w") as file:
		out  = f"#define ALLOWED_INDEX_COUNT {len(index)}\n"
		out += f"#define ALLOWED_COUNT {count}\n"
		out += f"#define ANSWER_COUNT {answer_count}"
		file.write(out)
	with open("Src/allowedlist.bin", "wb") as file:
		file.write(binary)
	with open("Src/allowedindex.bin", "wb") as file:
		file.write(bytearray(index))
	with open("Src/answerlist.bin", "wb") as file:
		file.write(bytearray(answers.encode("ascii")))