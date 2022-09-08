#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

typedef enum {
  INTEGER,
  STRING,
  BOOLEAN,
} DATA_TYPE;

void read(int count, ...) {

  va_list integers;
  va_start(integers, count);

  char buf[32];
  int i = 0;
  for (i = 0; i < count; i++) {

    int *cur = va_arg(integers, int*);
    fgets(buf, sizeof(buf), stdin);
    *cur = strtol(buf, NULL, 0);
  }
  
  va_end(integers);
}


void write(int count, ...) {

  va_list expressions;
  va_start(expressions, count);
  
  while(--count >= 0) {
    //first get type
    DATA_TYPE type = va_arg(expressions, DATA_TYPE);

    switch(type) {
    case INTEGER: {
	int val = va_arg(expressions, int);
	printf("%d", val);
      } break;
    case STRING: {
	char *str = va_arg(expressions, char *);
	printf("%s", str);
      } break;
    case BOOLEAN: {
	
	int val = va_arg(expressions, int);
	if (val)
	  printf("true");
	else
	  printf("false");
	
      } break;
    default:
      printf("Wrong type: %d\n", type);
      break;
    }

    //there is a space in between each expression
    if (count > 0)
      printf(" ");
  }
  //all write statements conclude with a newline character
  printf("\n");

  va_end(expressions);
}



