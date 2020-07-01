#/usr/bin/perl -W
# set sv_login_helper to "perl /path/to/this/script.pl"
sub read_message
{
    my $status = sysread(STDIN, my $raw_size, 4);
    if ($status == 0) {		# eof
	exit(1);
    }

    my $size = unpack("L", $raw_size);

    $status = sysread(STDIN, my $msg, $size);
    if ($status == 0) {		# eof
	exit(1);
    }

    return substr($msg, 5);
}

sub write_message
{
    my $msg = shift;
    my $size = length($msg);
    my $raw_size = pack("L", $size);
    syswrite(STDOUT, $raw_size, 4);
    syswrite(STDOUT, $msg, $size);
}

write_message("PRINTPerl example login helper v0.1\nUsername>");
write_message("INPUT");
my $user = read_message();

write_message("PRINTPassword>");
write_message("INPUT");
my $pass = read_message();

if ($user ne "mihawk" || $pass ne "hunter2") {
    exit(1);
}

write_message("LOGIN");
write_message("PRINT&c07fLogin successfull!");

# Mainloop
while(1) {
    my $msg = read_message();
    write_message("PRINT${msg}");
}
