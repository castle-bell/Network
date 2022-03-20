#include <stdio.h>
int main(int argc, char** argv)
{
    printf("Welcome to DataFlair tutorials!\n\n");

    int i;
    printf("The number of arguments are: %d\n",argc);
    printf("The arguments are:");
    for(i = 0; i <argc; i++)
    {
        printf("%s ", argv[i]);
    }
    printf("\n");
    return 0;
}