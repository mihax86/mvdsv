#login helper

##What is it?
The login helper is a program that enables the server admin to control
the connected client's logon flow from the beggining to the end of the session.

The main goal of the _login-helper_ is to enable server admins gain more control
of their servers without having to deal with MVDSV sourcecode directly, and with
the advantage of doing this by writting his helper program in any programming
language he wants.

Another good thing is that the login-helper will be spawned in a separate context
from MVDSV, so the developer of the helper doesn't have to worry about the
server's mainloop or anything complicated.

##How it works
By setting sv\_login to a value greater than 0 and setting sv\_login\_helper
to the helper program you want, every non-VIP player that tries to connect
to the server, will spawn an independent helper program that will control
the login flow of the user till the end of the session (user drops), if the
user drops the login program must check for EOF on the STDIN pipe and in case
the login helper program exits, the server will automatically disconnect the
user.

##How to write a login-helper program
###Packet format

packet = [size (4 bytes unsigned long machine byte order)][data_section (size bytes)]

data_section = [opcode (5 bytes)][opcode_data]

####Reading data
You must parse the packet according to the format above. Read the first 4 bytes
to know the size of the packet, and then read the rest of the packet using the
size you just parsed. And don't worry about endianness, it's on the machine
native byte order.

After that you must deal with the "data\_section" which contains a 5 byte string
(opcode) followed by the "opcode\_data" (if any).

####Writting data
For writting just follow the same convention above, build the data_section of
your packet and then calculate the total size of this data, send it first as
a unsigned long in the native byte order, then send the packet data.

###Supported opcodes

* SINFO - Requests the serverinfo string (reply is received by SINFO opcode);
* UINFO - Requests the userinfo string (reply is received by UINFO opcode);
* SAUTH - Sets the user *auth star infokey (no reply);
* PRINT - Prints a high priority message in the user's screen (no reply);
* CPRNT - Prints a message in the center of the user's screen (no reply);
* BCAST - Broadcasts a message to all connected clients (no reply);
* SVCMD - Issues a command on the server console (output from the command is
received by SVOUT)
* CLCMD - Issues a command on the client's console (no reply);
* INPUT - Requests input from the user (that will make the next thing the user
say be invisible and sent directly to the helper program);
* LOGIN - That will enable the user to enter the server (otherwise the user
gets stuck in the beggining of the connection process);
* CLOUT - Any output coming from the user comes from this opcode;
* SVOUT - Any output coming from commands executed on the server comes from this
variable;
* SVEND - This opcode marks the end of a command executed on the server-side.

###Samples

There are sample helper programs written in some well-known languages. I think
the most elaborate example as of now is the PHP one, the others are quite simple.

They can be found in the contrib folder of this project.


