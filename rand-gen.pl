#!/usr/bin/perl
use strict;
use warnings;
use List::Util qw(shuffle);

my $rank = shift || 1024;
my $repeat = shift || 5;

for (my $i = 0; $i < $repeat; $i++) {
	my @list = shuffle(0..($rank - 1));
	for (my $j = 0; $j < $rank; $j++) {
		print $list[$j],"\n";
	}
}

