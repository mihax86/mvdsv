#!/usr/bin/env python
# set sv_login_helper to "python /path/to/this/script.py"
import struct
import sys

def read_message():
    raw_size = sys.stdin.read(4)
    if len(raw_size) == 0:
        exit(1) # EOF

    size = struct.unpack('@I', raw_size)[0]
    msg = sys.stdin.read(size).decode("UTF-8")
    return msg[5:] # skip opcode

def write_message(msg):
    content = msg.encode("UTF-8")
    content_len = struct.pack('@I', len(content))
    sys.stdout.write(content_len)
    sys.stdout.write(content)
    sys.stdout.flush()

# Print MOTD for this helper
write_message("PRINTHello this is the python example login helper program v0.000000000001!")

# Print the user information
write_message("UINFO")
userinfo = read_message()
write_message("PRINTYour userinfo: " + userinfo)

write_message("PRINT&c0f0Type your username:")
write_message("INPUT")
username = read_message()

write_message("PRINT&c0f0Now your password:")
write_message("INPUT")
password = read_message()

if username != "mihawk" or password != "hunter2":
    write_message("PRINTLogin failed")
    sys.exit(0)

# Set some extra user information
write_message("SAUTHmihawk")
# Authorize user to login
write_message("LOGIN")
# Broadcast message to all users
write_message("BCASTHave no fear " + username[:-1] + " is here!")

# Mainloop
while True:
    msg = read_message()
    write_message("PRINT" + msg)
