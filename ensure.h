#define ensure(test,msg) if(!test) { perror(msg); abort(); }
#define ensure0(iszero) ensure(0==iszero,#iszero " is not zero!")
