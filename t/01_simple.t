use strict;
use warnings;
use utf8;
use Test::More;
use t::Util;

filter {
    statements($_[0]);
};

test( '33', int_(33));
test( '.33', number(0.33));
test( '0.33', number(0.33));
test( '3.14', number(3.14));
test( '0b1000', int_(8));
test( '0b0100', int_(4));
test( '0b0010', int_(2));
test( '0b0001', int_(1));
test( '0xdeadbeef', int_(0xdeadbeef));
test( '0o755', int_(0755));

test( '-5963', int_(-5963));
test( '3*4', mul(int_(3), int_(4)));
test( '3/4', div(int_(3), int_(4)));
test( '3+4', add(int_(3), int_(4)));
test( '3-4', sub_(int_(3), int_(4)));
test( '3-4-2', sub_(sub_(int_(3), int_(4)), int_(2)));
test( '3+4-2', sub_(add(int_(3), int_(4)), int_(2)));
test( '3+4*2', add(int_(3), mul(int_(4), int_(2))));

test( 'say()', funcall(ident('say'), args()));
test( 'say(3)', funcall(ident('say'), args(int_(3))));

test( '"hoge"', string('hoge'));
test( '"ho\nge"', string("ho\nge"));

filter { $_[0] };
test( '1;2"', statements(int_("1"), int_("2")));

done_testing;

