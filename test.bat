rem Mess-DOS batch file to check program correctness
mix <prime.mix >foo
echo n | comp prime.out foo
mix <mystery.mix >foo
echo n | comp mystery.out foo
