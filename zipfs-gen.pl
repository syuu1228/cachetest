#!/usr/bin/perl
use strict;
use warnings;
my $rank = shift || 1024;
my $repeat = shift || 5;
my $req = $rank * $repeat;

my $sum;
my @sums;
for (my $i = 0; $i < $rank; $i++) {
    $sum += 1/($i+1);
    $sums[$i] = $sum;
}

for (my $i = 0; $i < $req; $i++) {
    my $value = rand() * $sum;
    my $idx = bsearch($value);
    print $idx,"\n";
}

sub bsearch {
    my ($value) = @_;
    my $head = 0;
    my $tail = $rank - 1;
    while ($head <= $tail) {
        my $where = int(($head + $tail) / 2);
        if ($value <= $sums[$where]) {
            $tail = $where - 1;
        } else {
            $head = $where + 1;
        }
    }
    return $head;
}
