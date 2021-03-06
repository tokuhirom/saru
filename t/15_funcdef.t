use strict;
use warnings;
use utf8;
use Test::More 0.98;
use Test::Base::Less;
use t::Util;

run_test_cases(blocks);

done_testing;

__END__

===
--- code
sub sum($a, $b) { $a+$b }; say(sum(3,5))
--- expected
8


===
--- code
sub sum($a, $b=9) { $a+$b }
say(sum(3))
say(sum(7, 6))
--- expected
12
13

===
--- code
sub s($b=5963) { $b }; say(s())
--- expected
5963

===
--- code
sub s($b=5963) { $b }; say(s(4649))
--- expected
4649
