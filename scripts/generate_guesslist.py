#!/usr/bin/python3

def word_to_b26(word):
	i = 0
	for c in word.strip():
		i *= 26
		i += int(ord(c) - ord('a'))
	return i

if __name__=="__main__":
	binary = bytearray()
	count = 0
	with open("scripts/allowed-guesses.txt", "r") as file:
		for line in file:
			if (line.strip()):
				num = word_to_b26(line)
				binary.extend(num.to_bytes(3, byteorder='big'))
				count += 1
	with open("Src/allowedlist.h", "w") as file:
		out = f"const unsigned int allowed_count = {count};\n"
		out += f"const unsigned char allowed_list[] = {{{', '.join([hex(h) for h in binary])}}};"
		file.write(out)