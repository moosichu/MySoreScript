var i = 0;
func name (){
    var a = 1;
    while(i < 1000000) {
        i = i + a;
    }
    return i;
};

i = name();
i = name();
i = name();
i.dump();


