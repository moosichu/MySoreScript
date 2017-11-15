func name (a, b, c, d){
    var x = a + b;
    var y = c + d;
    var z = x + y;
    var z = a + b + c + d + x + y + z;
    return z;
};

var i = 0;

while(i < 100000) {
    i = i + 1;
    j = name(1, 2, 3, 4);
}

j.dump();


a = name("hello", "how", "are", "you\n");
a.dump();


