<?php

// set sv_login_helper to "php /path/to/this/script.php"
//==============================================================================
//
// Communication functions
//
// Check:
// https://www.php.net/manual/en/function.pack.php
// https://www.php.net/manual/en/function.unpack.php
//
// for reference on using pack() and unpack()
//
//==============================================================================
function write_message($msg)
{
    $msgsize = pack("L", strlen($msg)); // unsigned long (always 32 bit, machine byte order)
    fwrite(STDOUT, $msgsize);           // write packed uint32 (4 bytes)
    fwrite(STDOUT, $msg);               // write data
    fflush(STDOUT);                     // make sure data is transmitted imediatelçlçy
}

function read_message_ex()
{
    $rawsize = fread(STDIN, 4); // read first 4 bytes (unsigned long)

    // Detect EOF (This is important, since that will indicate when the
    // client dropped from the server, if you don't check for errors or
    // anything during reads or writes this helper might continue to run
    // forever).
    if ($rawsize == FALSE) {
        // The user has disconnected from the server (pipe closed)
        if (feof(STDIN))
            die();
    }

    $size = unpack("L", $rawsize)[1]; // unpack binary data back in PHP
    $data = fread(STDIN, $size);      // read data based on the $size we got
    if ($data == FALSE) {
        // The user has disconnected from the server (pipe closed)
        if (feof(STDIN))
            die();
    }

    return array(substr($data, 0, 5), substr($data, 5)); // make an array containing
                                                         // the opcode at index [0]
                                                         // and the message at index [1];
}

// This one strips the opcode (when you don't care)
function read_message()
{
    return read_message_ex()[1];
}

//==============================================================================
//
// Utils
//
//==============================================================================

function random_string($length = 11)
{
    $chars = '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ';
    $chars_len = strlen($chars);
    $str = '';

    for ($i = 0; $i < $length; $i++) {
        $str .= $chars[rand(0, $chars_len - 1)];
    }

    return $str;
}

// Check if the user is allowing snaps on his client, this is a bit tricky
function check_scr_allowsnap()
{
    // Request input (will make the client output his latest say() to us
    // and not to the server)
    write_message("INPUT");

    // Here we generate a random string so we don't get the wrong client
    // output when receiving the information we requested
    $check_string = random_string(11);
    $check_string .= ": ";
    $check_length = strlen($check_string);

    // Send the variable scr_allowsnap value in this INPUT request
    write_message('CLCMDsay ' . $check_string . '$scr_allowsnap');

    // Read the response
    $msg = read_message_ex();

    // As of this implementation, you have to make sure you call it correctly
    // in your code so that you don't lose data
    while ($msg[0] != "CLOUT"
            || substr($msg[1], 0, $check_length) != $check_string)

        // Read again until we get what we want (we may get stuck here)
        $msg = read_message_ex();

    // Parse the received integer
    if (intval(substr($msg[1], $check_length, 1)) == 0) {
        write_message("PRINT&cf00scr_allowsnap must be enabled in order to join this server.\nSorry!");
        die();
    }

    // Allow snap is set
    return 0;
}

//==============================================================================
//
// Login process
//
//==============================================================================

// Note the &cxxx syntax, that will only work when the client has colortext support
// (ezquake and friends). You can also go figure out the binary values for
// special text in quake and throw it on a PRINT command.
write_message("PRINT\n&c07f[PHP] &c0f0Login module &cf00v0.01");

// Check for allow snap
check_scr_allowsnap();

write_message("PRINT\nValid credentials are:\n&cff0username: <whatever>\npassword: hunter2\n");

// Get username
write_message("PRINTType your username:");
write_message("INPUT");
$user = read_message();

// Get password
write_message("PRINTType your password $user:");
write_message("INPUT");
$pass = read_message();

// Check credentials
if ($user == $user && $pass == "hunter2") {
    write_message("LOGIN");         // allow user to join
    write_message("SAUTH$user");    // set *auth infokey

    // So that everyone has the hability to know some more if they
    // want to.
    write_message("CLCMDallow_f_cmdline 1");
    write_message("CLCMDallow_f_system 1");
    write_message("PRINTYour allow_f_cmdline and allow_f_system are now set to 1.");

    // Tell everybody who just joined the party.
    write_message("BCAST$user logged in successfully.");
}
else {
    write_message("PRINT&cf70Login failed!");
    die();
}

//==============================================================================
//
// Mainloop
//
//==============================================================================
$say_hello = true;

while (true) {

    // Wait 3 seconds for data
    $read = array(STDIN);
    $write = NULL;
    $except = NULL;
    $status = stream_select($read, $write, $except, 3);

    // Data arrived
    if ($status > 0) {
        $msg = read_message_ex();

        // Debug print
        write_message("PRINTReceived opcode: [$msg[0]] data: [$msg[1]]");

        // Only Client output is parsed as a possible command
        if ($msg[0] == "CLOUT") {
            // Some custom command for disconnecting the client
            if ($msg[1] == "!bye")
                break;

            // Tell them something !about this thing
            if ($msg[1] == "!about")
                write_message("PRINT&c07fLogin module written by mihawk 2020.");

            // Toggle annoyance
            if ($msg[1] == "!hello") {
                $say_hello = !$say_hello;
                if ($say_hello)
                    write_message("PRINT&c07fAnnoyance enabled!");
                else
                    write_message("PRINT&c07fAnnoyance disabled!");
            }
        }

        continue;
    }

    // Check again for allowsnap
    check_scr_allowsnap();

    // Annoy the player for fun.
    if ($say_hello) {
        write_message("CPRNTHello $user! in your face");
        write_message("PRINTHello $user! for you only"); // On the user's console
        write_message("BCASTHello $user! for everybody"); // To all users
        write_message("SVCMDsay Hello $user! from server");
        write_message("CLCMDsay Hello $user! from myself");
    }

}

write_message("PRINT&cf00You were disconnected!!!");
