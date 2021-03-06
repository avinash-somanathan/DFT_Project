/*=======================================================================

GROUP NUMBER - 20
Team Members -
1. Abhinav Chilakamarri
2. Pravin Nandagopal
3. Musaddique Ansari
4. Avinash Somanathan

  A simple parser for "self" format

  The circuit format (called "self" format) is based on outputs of
  a ISCAS 85 format translator written by Dr. Sandeep Gupta.
  The format uses only integers to represent circuit information.
  The format is as follows:

1        2        3        4           5           6 ...
------   -------  -------  ---------   --------    --------
0 GATE   outline  0 IPT    #_of_fout   #_of_fin    inlines
                  1 BRCH
                  2 XOR(currently not implemented)
                  3 OR
                  4 NOR
                  5 NOT
                  6 NAND
                  7 AND

1 PI     outline  0        #_of_fout   0

2 FB     outline  1 BRCH   inline

3 PO     outline  2 - 7    0           #_of_fin    inlines




                                    Author: Chihang Chen
                                    Date: 9/16/94

=======================================================================*/

/*=======================================================================
  - Write your program as a subroutine under main().
    The following is an example to add another command 'lev' under main()

enum e_com {READ, PC, HELP, QUIT, LEV};
#define NUMFUNCS 5
int cread(), pc(), quit(), lev();
struct cmdstruc command[NUMFUNCS] = {
   {"READ", cread, EXEC},
   {"PC", pc, CKTLD},
   {"HELP", help, EXEC},
   {"QUIT", quit, EXEC},
   {"LEV", lev, CKTLD},
};

lev()
{
   ...
}
=======================================================================*/

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <cstring>
#include <vector>
#include <iostream>

using namespace std;
#define MAXLINE 81               /* Input buffer size */
#define MAXNAME 31               /* File name size */

#define Upcase(x) ((isalpha(x) && islower(x))? toupper(x) : (x))
#define Lowcase(x) ((isalpha(x) && isupper(x))? tolower(x) : (x))

enum e_com {READ, PC, HELP, LEVEL, SIMUL, QUIT};
enum e_state {EXEC, CKTLD};         /* Gstate values */
enum e_ntype {GATE, PI, FB, PO};    /* column 1 of circuit format */
enum e_gtype {IPT, BRCH, XOR, OR, NOR, NOT, NAND, AND};  /* gate types */

struct cmdstruc {
   char name[MAXNAME];        /* command syntax */
   int (*fptr)();             /* function pointer of the commands */
   enum e_state state;        /* execution state sequence */
};

typedef struct n_struc {
   unsigned indx;             /* node index(from 0 to NumOfLine - 1 */
   unsigned num;              /* line number(May be different from indx */
   enum e_gtype type;         /* gate type */
   unsigned fin;              /* number of fanins */
   unsigned fout;             /* number of fanouts */
   struct n_struc **unodes;   /* pointer to array of up nodes */
   struct n_struc **dnodes;   /* pointer to array of down nodes */
   int level;                 /* level of the gate output */
   int value;
} NSTRUC;                     

/*----------------- Command definitions ----------------------------------*/
#define NUMFUNCS 6
int cread(), pc(), help(), quit(), lev();
int clear(), allocate(),simulate(), evaluate(NSTRUC *np);
struct cmdstruc command[NUMFUNCS] = {
   {"READ", cread, EXEC},
   {"PC", pc, CKTLD},
   {"HELP", help, EXEC},
   {"LEVEL", lev, CKTLD},
   {"SIMUL", simulate,CKTLD},
   {"QUIT", quit, EXEC},
};

/*------------------------------------------------------------------------*/
enum e_state Gstate = EXEC;     /* global exectution sequence */
NSTRUC *Node;                   /* dynamic array of nodes */
NSTRUC **Pinput;                /* pointer to array of primary inputs */
NSTRUC **Poutput;               /* pointer to array of primary outputs */
int Nnodes;                     /* number of nodes */
int Npi;                        /* number of primary inputs */
int Npo;                        /* number of primary outputs */
int Done = 0;                   /* status bit to terminate program */
char *cp;
/*------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: shell
description:
  This is the main program of the simulator. It displays the prompt, reads
  and parses the user command, and calls the corresponding routines.
  Commands not reconized by the parser are passed along to the shell.
  The command is executed according to some pre-determined sequence.
  For example, we have to read in the circuit description file before any
  action commands.  The code uses "Gstate" to check the execution
  sequence.
  Pointers to functions are used to make function calls which makes the
  code short and clean.
-----------------------------------------------------------------------*/
main()
{
   enum e_com com;
   char cline[MAXLINE], wstr[MAXLINE];

   while(!Done) {
      printf("\nCommand>");
      fgets(cline, MAXLINE, stdin);
      if(sscanf(cline, "%s", wstr) != 1) continue;
      cp = wstr;
      while(*cp){
	*cp= Upcase(*cp);
	cp++;
      }
      cp = cline + strlen(wstr);
      com = READ;
      while(com < NUMFUNCS && strcmp(wstr, command[com].name)) 
	com = static_cast<e_com>(static_cast<int>(com) + 1);
      if(com < NUMFUNCS) {
         if(command[com].state <= Gstate) (*command[com].fptr)();
         else printf("Execution out of sequence!\n");
      }
      else system(cline);
   }
}

/*-----------------------------------------------------------------------
input: circuit description file name
output: nothing
called by: main
description:
  This routine reads in the circuit description file and set up all the
  required data structure. It first checks if the file exists, then it
  sets up a mapping table, determines the number of nodes, PI's and PO's,
  allocates dynamic data arrays, and fills in the structural information
  of the circuit. In the ISCAS circuit description format, only upstream
  nodes are specified. Downstream nodes are implied. However, to facilitate
  forward implication, they are also built up in the data structure.
  To have the maximal flexibility, three passes through the circuit file
  are required: the first pass to determine the size of the mapping table
  , the second to fill in the mapping table, and the third to actually
  set up the circuit information. These procedures may be simplified in
  the future.
-----------------------------------------------------------------------*/
int cread()
{
   char buf[MAXLINE];
   int ntbl, *tbl, i, j, k, nd, tp, fo, fi, ni = 0, no = 0;
   FILE *fd;
   NSTRUC *np;

   sscanf(cp, "%s", buf);
   if((fd = fopen(buf,"r")) == NULL) {
      printf("File %s does not exist!\n", buf);
      return 1;
   }
   if(Gstate >= CKTLD) clear();
   Nnodes = Npi = Npo = ntbl = 0;
   while(fgets(buf, MAXLINE, fd) != NULL) {
      if(sscanf(buf,"%d %d", &tp, &nd) == 2) {
	printf("%d %d\n", tp, nd);
         if(ntbl < nd) ntbl = nd;
         Nnodes ++;
         if(tp == PI) Npi++;
         else if(tp == PO) Npo++;
      }
   }
   tbl = (int *) malloc(++ntbl * sizeof(int));

   fseek(fd, 0L, 0);
   i = 0;
   printf("Tbl values printing here\n\n");
   while(fgets(buf, MAXLINE, fd) != NULL) {
      if(sscanf(buf,"%d %d", &tp, &nd) == 2) tbl[nd] = i++;
   }
   allocate();

   printf("Node values printing here\n\n");

   fseek(fd, 0L, 0);
   while(fscanf(fd, "%d %d", &tp, &nd) != EOF) {
   
      printf("%d %d\t",nd,tbl[nd]);
      np = &Node[tbl[nd]];
      np->num = nd;
      if(tp == PI) Pinput[ni++] = np;
      else if(tp == PO) Poutput[no++] = np;
      switch(tp) {
         case PI:
         case PO:
         case GATE:
            fscanf(fd, "%d %d %d", &np->type, &np->fout, &np->fin);
            break;
         
         case FB:
            np->fout = np->fin = 1;
            fscanf(fd, "%d", &np->type);
            break;

         default:
            printf("Unknown node type!\n");
            exit(-1);
         }
      np->unodes = (NSTRUC **) malloc(np->fin * sizeof(NSTRUC *));
      np->dnodes = (NSTRUC **) malloc(np->fout * sizeof(NSTRUC *));
      for(i = 0; i < np->fin; i++) {
         fscanf(fd, "%d", &nd);
         np->unodes[i] = &Node[tbl[nd]];
         }
      for(i = 0; i < np->fout; np->dnodes[i++] = NULL);

      printf("Value of nd = %d IDX= %d fin=%d fout=%d\n",np->num,np->indx,np->fin,np->fout);
      }
   for(i = 0; i < Nnodes; i++) {
      for(j = 0; j < Node[i].fin; j++) {
         np = Node[i].unodes[j];
         k = 0;
         while(np->dnodes[k] != NULL) k++;
         np->dnodes[k] = &Node[i];
         }
      }
   fclose(fd);
   Gstate = CKTLD;
   printf("==> OK\n");
}

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: main
description:
  The routine prints out the circuit description from previous READ command.
-----------------------------------------------------------------------*/
int pc()
{
   int i, j;
   NSTRUC *np;
   char *gname(int tp);
   
   printf(" Node   Type	  level \tIn     \t\t\tOut    \n");
   printf("------ ------  ----- \t-------\t\t\t-------\n");
   for(i = 0; i<Nnodes; i++) {
      np = &Node[i];
      printf("\t\t\t\t\t");
      for(j = 0; j<np->fout; j++) printf("%d ",np->dnodes[j]->num);
      printf("\r%5d %s %d\t", np->num,gname(np->type),np->level);
      for(j = 0; j<np->fin; j++) printf("%d ",np->unodes[j]->num);
      printf("\n");
   }
   printf("Primary inputs:  ");
   for(i = 0; i<Npi; i++) printf("%d ",Pinput[i]->num);
   printf("\n");
   printf("Primary outputs: ");
   for(i = 0; i<Npo; i++) printf("%d ",Poutput[i]->num);
   printf("\n\n");
   printf("Number of nodes = %d\n", Nnodes);
   printf("Number of primary inputs = %d\n", Npi);
   printf("Number of primary outputs = %d\n", Npo);
}

int lev()
{
int k=0, old_k=0,i,j,m;
   NSTRUC *np;
   for(i = 0; i<Nnodes; i++) {
      np = &Node[i];
      if(np->type==0) np->level=0;
      else np->level= -1;
   }
   for(i=0; i<Nnodes; i++){
      np = &Node[i];
      if(np->type==0) continue;
      else if(np->type==1){ 
	     if(np->unodes[0]->level!=-1) 
		np->level = np->unodes[0]->level + 1;
	   }
      else {
	int flag=1;

	for(j=0; j < np->fin; j++){
	
	  if(np->unodes[j]->level!=-1){
		old_k=k;
		k = np->unodes[j]->level;
		if(old_k > k) k = old_k;
		printf("%d  %d\n",np->unodes[j]->num, np->unodes[j]->level);
	  }
	  else flag = 0;
	 }
	 if(flag==1) np->level=k+1;
	}
    }
}

	struct NV {
		NSTRUC *nd;
		int new_val;
	} NV_Pair;

int simulate(){
	int delay=2,old_fout=0;
	vector<vector<NV> > ts(Npi);
	vector<int> test_vec(Npi);
	ts.resize(1);
	test_vec[0]=0;
		ts[0].push_back({Pinput[0],test_vec[0]});
	for(int i=1; i<Npi; i++){
		NSTRUC *np=Pinput[i];
		test_vec[i] = ~test_vec[i-1];
		ts[0].push_back({np,test_vec[i]});
		cout <<" expected value of PI " << test_vec[i] << endl;
			
		}
	for(int j=0; j< ts.size(); j++){
		cout << "Current value of j " << j << " size of j " << ts.size() << endl;
			if(ts[j][0].nd==NULL) continue;
		for(int k=0; k < ts[j].size(); k++){
			cout << "Current value of k " << k << "size of k " << ts[j].size() << endl;
			NSTRUC *new_np = ts[j][k].nd;
			new_np->value = ts[j][k].new_val;
        	            if(new_np->fout!=0)
			    { 
				old_fout = old_fout + new_np->fout; 
			//	cout <<"trying to resize"<<endl; 
				ts.resize(j+delay+1);
                                ts[j+delay-1].resize(old_fout);
	          		for(int m=0; m < new_np->fout; m++){
		        		if(new_np->dnodes[m]->type==1) {
						ts[j].push_back({new_np->dnodes[m],new_np->value});
					}
			        	else {
						ts[j+delay].push_back({new_np->dnodes[m],evaluate(new_np->dnodes[m])});
			         		std::cout << "new_np = " << new_np->num << " new_np_dnodes = " << new_np->dnodes[m]->num << " evaluated result = " << evaluate(new_np->dnodes[m]) << endl;
				}
			}	
		}
	}
}

for(int i=0; i< Nnodes; i++){
NSTRUC *np = &Node[i];

cout << "Value of node "<< np->num << " is " << np->value << endl;
}
}

int evaluate( NSTRUC *np){
cout <<"Entered Evaluate\n";
vector<int> input;
	for(int i=0;i< np->fin; i++){
	input.push_back(np->unodes[i]->value);
	}


 switch(np->type) {
    case 0: return 0;
    case 1: return 0;
    case 2: {
		int ret_val=0;
		for(int i=0;i<input.size();i++){
			ret_val = ret_val ^ input[i];
		}
		return ret_val;
	    }
    case 3:{
               int ret_val=0;
               for(int i=0;i<input.size();i++){
	       	ret_val = ret_val | input[i];     
               }
               return ret_val;
           }
    case 4:{
               int ret_val=0;
               for(int i=0;i<input.size();i++){
               	ret_val = (ret_val | input[i]);     
               }
               return ~ret_val;
           }
 
    case 5:{
                int ret_val=0;
                for(int i=0;i<input.size();i++){
                	ret_val = ~ input[i];     
                }
                return ret_val;
            }

    case 6: {
                 int ret_val=0;
                 for(int i=0;i<input.size();i++){
                 	ret_val = (ret_val | ~input[i]);     
                 }
                 return ret_val;
             }

    case 7: {
                 int ret_val=1;
                 for(int i=0;i<input.size();i++){
                 	ret_val = (ret_val & input[i]);     
                 }
                 return ret_val;
	     }           
	}
return 0;
}
	
/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: main 
description:
  The routine prints ot help inormation for each command.
-----------------------------------------------------------------------*/
int help()
{
   printf("READ filename - ");
   printf("read in circuit file and creat all data structures\n");
   printf("PC - ");
   printf("print circuit information\n");
   printf("HELP - ");
   printf("print this help information\n");
   printf("LEVEL - ");
   printf("levelize the circuit\n");
   printf("SIMUL - ");
   printf("event driven simulation\n");
   printf("QUIT - ");
   printf("stop and exit\n");
}

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: main 
description:
  Set Done to 1 which will terminates the program.
-----------------------------------------------------------------------*/
int quit()
{
   Done = 1;
}

/*======================================================================*/

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: cread
description:
  This routine clears the memory space occupied by the previous circuit
  before reading in new one. It frees up the dynamic arrays Node.unodes,
  Node.dnodes, Node.flist, Node, Pinput, Poutput, and Tap.
-----------------------------------------------------------------------*/
int clear()
{
   int i;

   for(i = 0; i<Nnodes; i++) {
      free(Node[i].unodes);
      free(Node[i].dnodes);
   }
   free(Node);
   free(Pinput);
   free(Poutput);
   Gstate = EXEC;
}

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: cread
description:
  This routine allocatess the memory space required by the circuit
  description data structure. It allocates the dynamic arrays Node,
  Node.flist, Node, Pinput, Poutput, and Tap. It also set the default
  tap selection and the fanin and fanout to 0.
-----------------------------------------------------------------------*/
int allocate()
{
   int i;

   Node = (NSTRUC *) malloc(Nnodes * sizeof(NSTRUC));
   Pinput = (NSTRUC **) malloc(Npi * sizeof(NSTRUC *));
   Poutput = (NSTRUC **) malloc(Npo * sizeof(NSTRUC *));
   for(i = 0; i<Nnodes; i++) {
      Node[i].indx = i;
      Node[i].fin = Node[i].fout = 0;
   }
}

/*-----------------------------------------------------------------------
input: gate type
output: string of the gate type
called by: pc
description:
  The routine receive an integer gate type and return the gate type in
  character string.
-----------------------------------------------------------------------*/
char *gname(int tp)
{
   switch(tp) {
      case 0: return("PI");
      case 1: return("BRANCH");
      case 2: return("XOR");
      case 3: return("OR");
      case 4: return("NOR");
      case 5: return("NOT");
      case 6: return("NAND");
      case 7: return("AND");
   }
}
/*========================= End of program ============================*/

