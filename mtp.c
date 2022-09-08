/*
 * CMPT 399 (Winter 2016)
 * Assignment 4: Code Generation
 * Author: Derrick Gold
 *
 * This file contains the program entry for the MacEwan Teeny Pascal
 * compiler. It includes all the necessary modules such as the lexer, parser, etc...
 * and glues them together creating a compiler.
 *
 */
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h> //include for getopt
#include <libgen.h> //include for basename


#include "parser.h"
#include "lexer.h"
#include "tree.h"
#include "parserHelper.h"
#include "analyze.h"
#include "codegen.h"

#define DO_VERBOSE_LEXER(verbose) ((verbose) > 2)
#define DO_VERBOSE_PARSER(verbose) ((verbose) > 1)
#define DO_VERBOSE_SEMANTIC(verbose) ((verbose) > 0)

#define DEFAULT_ASMOUT "program.s"
#define OUTFILE_EXT ".s"

//programs help/usage text
#define HELP_TEXT                                                       \
  ("\nUsage: %s [options] file\n\n"                                     \
   "Compile programs written in the MacEwan Teeny Pascal programming\nlanguage.\n\n" \
   "Options:\n"                                                         \
   "\t-h\t\tdisplay this help and exit\n"                               \
   "\t-v\t\tdisplay extra (verbose) debugging information\n"            \
   "\t\t\t(multiple -v options increase verbosity)\n")


//store the program's binary name
static char *_prgmName = NULL;


#define ERR_TOK_MSG(token, output) do {                  \
    fprintf(output, "Invalid token detected: (line ");   \
    Lexer_printToken(token, output);                     \
    fprintf(output, ")\n");                              \
  } while (0)

#define SYSERR_TOK_MSG(output) do {                                     \
    fprintf(output, "Lexer has encounterd a critical error: memory alloc failed.\n"); \
  } while (0)

/*
 * printHelp:
 *
 * Prints the usage text for the MacEwan Teeny Pascal program.
 */
static void printHelp(void) {

  if (!_prgmName) {
    //this shouldn't happen
    fprintf(stderr, "Failed to print help, program name could not be determined.");
    return;
  }

  printf(HELP_TEXT, _prgmName);
}

/*
 * So far, tokenizes input from a given file using flex,
 * parses the tokens with lemon, and runs semantic checks
 * on the AST.
 * Soon to be a completely working compile function.
 */
static int compile(FILE *asmOut, int verbose) {

  if (!asmOut)
    return EXIT_FAILURE;
  
  int returnVal = EXIT_SUCCESS;

  //initialize the parser
  void *parser = ParseAlloc(malloc);
  int type = 0;
  LexToken_t *tok = NULL;

  //Loop through input file, tokenize, and parse
  do {
    //get token
    LexToken_t token = Lexer_getToken();
    type = token.type;

    //print out the token if -vv is used    
    if (DO_VERBOSE_LEXER(verbose)) {
      Lexer_printToken(token, stdout);
      fprintf(stdout, "\n");
    }

    //heapify token for storing in a node
    tok = Lexer_heapifyToken(token);
    if (!tok) break;

    //pass token into parser
    Parse(parser, token.type, tok);
    
  } while (!LEXTOKEN_ISEOF(type) && !Parser_hasError());

  //if there was an error in the lexer, set return status to fail
  if (type == TOK_SYSERR)
    returnVal = EXIT_FAILURE;

  //If there was an error in the Parser, set return status to fail
  if (Parser_hasError())
    returnVal = EXIT_FAILURE;
  //otherwise, print the tree if -v is used
  else if (DO_VERBOSE_PARSER(verbose))
    TreeNode_print(stdout, Parser_getTree(), false);

  //check if abstract syntax tree was properly generated
  //before analyzing
  if (returnVal != EXIT_FAILURE) {
    //run semantic checks
    Analyze_Semantics(Parser_getTree(), DO_VERBOSE_SEMANTIC(verbose));
    if (Analyze_GetStatus() != NONE)
      returnVal = EXIT_FAILURE; 
  }

  //check if semantics was successful before generating code
  if (returnVal != EXIT_FAILURE && CodeGen_process(asmOut, Parser_getTree(), Analyze_GetRodata()))
      returnVal = EXIT_FAILURE;
  
  /*
   * Clean up
   */
  //free the heapified EOF token that is not consumed by lemon
  if (type == TOK_ENDFILE && tok)
    Lexer_tokenDestructor(tok);

  //free any memory used in the semantic analysis
  Analyze_Cleanup();
  //free the abstract syntax tree + symbol tabls
  TreeNode_destroy(Parser_getTree());
  //free the lemon parser
  ParseFree(parser, free);
  //free up any memory used by the lexer
  yylex_destroy();

  return returnVal;
}


/*
 * main:
 *
 * Program entry point. Parses arguments, takes a file, puts that file through a lexer
 * that generates tokens, which then get put through a syntactic analysis (so far).
 *
 * More to come in assignment 3!
 */
int main(int argc, char *argv[]) {

  _prgmName = basename(argv[0]);

  //No user given arguments provided, print help
  if (argc < 2) {
    printHelp();
    return EXIT_FAILURE;
  }

  int verbose = 0;
  char *inputFile = NULL;
  char *outputFile = NULL;

  //loop through arguments and collect options
  int c;
  while ((c = getopt(argc, argv, "hvo:")) != -1) {

    switch (c) {
      case 'h':
        printHelp();
        return EXIT_SUCCESS;

      case 'v':
        verbose++;
        break;
    case 'o':
      outputFile = optarg;
      break;
      default:
        //unsupported arguments
        fprintf(stderr, "Invalid argument: %c\n", c);
        return EXIT_FAILURE;
    }
  }

  /*
   * Check to make sure there are enough arguments
   */
  if (optind >= argc) {
    fprintf(stderr, "Missing arguments.");
    return EXIT_FAILURE;
  }

  //get input file
  inputFile = argv[optind++];

  //no input file, kill the program
  if (!inputFile) {
    fprintf(stderr, "No file specified.\n\n");
    printHelp();
    return EXIT_FAILURE;
  }


  /*
   * If after we have grabbed the file path and there are more arguments
   * indicate that there are extra files.
   */
  if (optind < argc) {
    fprintf(stderr, "Too many file arguments.\n\n");
    printHelp();
    return EXIT_FAILURE;
  }

  /*
   * Open the file to put through the lexer.
   * It is necessary to create a copy of the file handle since
   * flex will set its copy of the file handle to 0 when it is 
   * finished.
   *
   * This way we still have a reference to close the file with.
   */
  FILE *inFile = fopen(inputFile, "r");
  if (!inFile) {
    fprintf(stderr, "Error opening input fle: %s\n\n", inputFile);
    return EXIT_FAILURE;
  }
  yyin = inFile;


  //set the default output file name if one hasn't been specified
  char outputBuf[strlen(inputFile) + strlen(OUTFILE_EXT) + 1];
  if (!outputFile) {
    //no outputfile specified, use the same name as the input file
    //with new extension
    outputFile = outputBuf;
    strcpy(outputFile, inputFile);
    char *ext = strrchr(outputFile, '.');
    //replace the file extension
    if (ext) {
      strcpy(ext, OUTFILE_EXT);
    } else
      //otherwise, add the extension if its missing
      strcat(outputFile, OUTFILE_EXT);
  }
  
  //try and open the output assembler file
  FILE *outFile = fopen(outputFile, "w");
  if (!outFile) {
    fprintf(stderr, "ERror opening assembly output file: %s\n", outputFile);
    return EXIT_FAILURE;
  }
  

  
  /*
   * Store lexer + parser status for future assignments
   * when more parts will be added after this point.
   */
  int compileStatus = compile(outFile, verbose);

  //close input file 
  fclose(inFile);
  //close output file
  fclose(outFile);

  return compileStatus;
}
