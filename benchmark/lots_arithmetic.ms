func name (a, b, c, d){
    var x = a + b;
    var y = c + d;
    var z = x + y;
    return z;
};

i = name(1, 2, 3, 4);
i.dump();

j = name("hello", "how", "are", "you\n");
j.dump();


