sub fib($n) {
    if $n <= 2 {
        return 1;
    } else {
        # return fib($n-1)+fib($n-2);
        return fib($n-1)+fib($n-2);
    }
}

say(fib(25));