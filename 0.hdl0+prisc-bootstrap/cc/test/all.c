int main() {
    int x = 5;
    int y = 3;
    putint(x + y);
    putint(x - y);
    putint(x * y);
    int i = 0;
    while (i < 3) {
        putint(i);
        i = i + 1;
    }
    int z = 10;
    if (z > 5) {
        putint(1);
    } else {
        putint(0);
    }
    return 0;
}
