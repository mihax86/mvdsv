#!/usr/bin/env ruby
def write_message(msg)
  a = [ msg.length ].pack("L")
  STDOUT.write(a.to_s + msg)
  STDOUT.flush()
end

def read_message()
  raw_size = STDIN.read(4)
  if raw_size == 0
    exit
  end
  size = raw_size.unpack("L")[0]
  return STDIN.read(size)[5..-1]
end

write_message("PRINTWelcome to mihawk's ruby login service v3.0000000000003\nType your username>")
write_message("INPUT")
user = read_message()
write_message("PRINTType your password>")
write_message("INPUT")
pass = read_message()

if user == "mihawk" and pass == "hunter2"
  write_message("BCASTmihawk is here again.")
  write_message("SAUTHmihawk")
  write_message("LOGIN")
else
  write_message("PRINT&cf00LOGIN FAILED!")
end

while true
  msg = read_message()
  write_message("PRINT#{msg}")
end
