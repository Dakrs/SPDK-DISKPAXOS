import string
import random
import sys

def id_generator(size=6, chars=string.ascii_uppercase + string.digits):
  return ''.join(random.choice(chars) for _ in range(size))

MAX_NUMBER_STR = int(sys.argv[1])

strings = []

for i in range(0,MAX_NUMBER_STR):
    strings.append(id_generator())

#print(strings)

NUM_PROCESSES = int(sys.argv[2])

for i in range(0,NUM_PROCESSES):
    f = open(f'example_files/input-{i}',"w+")
    for j in range(0,MAX_NUMBER_STR):
        f.write(strings[j]+'\n')
    random.shuffle(strings)
    f.close()
