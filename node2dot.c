/*
 * node2dot
 *
 *      Convert a node tree into dot format
 *	 
 *                    yshen 2016
 *
 * initial version 2016-9-19 12:00
 * last update 2016-9-19 add parmeter --skip-empty --color
 *  
 * The second change was made by Song Jinzhou, adding color control for the lines  2022-04-07 10:54:03
 * 
 * Third update: 2022-12-16 13:50:50
 * 		1. Bold the title
 * 		2. Relevant figures are displayed
 * 		3. Change the font
 */

#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<getopt.h>
#include<time.h>

#define LENGTH 1024
#define bool char
#define true 1
#define false 0

typedef struct Elem 
{
	char name[LENGTH];
} Elem;

typedef struct Node
{
	char name[LENGTH];
	Elem elems[LENGTH];
	char links[LENGTH][LENGTH];
	int links_count;
	char *color;
} Node;

/*
 * node_num and elem_num
 */
int node_num = 0;
int node_cnt = 0;
int elem_num = 0;

#define new(x) (++x)
#define set(var,value) (var=value)
#define get(x) (x)

/*
 * a mini-stack
 */
int *stack = NULL;
int top = 0;

int push(int var)
{
	stack[top++] = var;
}

int pop()
{
	return stack[--top];
}

void remove_line_breaks(char *p)
{
	int i = 0;

	while(*(p+i) != '\n')
	{
		i++;
	}
	*(p+i)='\0';
}

/*
 * globals
 */
Node nodes[LENGTH];
int add_node(int num, char *name);
int add_item(int node_n, int elem_n, char *name);
int add_link(int parent_node_num, int parent_elem_num, int child_node_num);
int print_header(void);
int print_footer(void);
int print_body(void);
char *get_one_name();

char *progname = "node2dot";

/*
 * options
 */
bool is_skip_empty = false;
bool is_color = false;
char *skip_node_name = NULL;

void usage()
{
	printf("\n\nConvert node tree of postgres to dot format\n"
		   "                  yshen 2016\n\n\n"
           "Usage:%s [options] <filein >fileout\n"
		   "Valid Options:\n"
		   "--skip-empty             do not print element if is 0 or null or false\n"
		   "--skip-node  nodename    do not print node named nodename\n"
		   "--color                  enable color output\n"
		   "--help                   show this help message\n"
			, progname);
}

char *linecolor[] = {"red","black","blue","skyblue","violet"};

int main(int argc, char *argv[])
{
	char buffer[LENGTH];
	
	int  parent_node_num;
	int  parent_elem_num;
	int  child_node_num;

    int c;
    int digit_optind = 0;

	/* 
	 * level indicates if we are in a struct context or not 
	 * if we are inside a struct level > 0 else level <= 0
	 * if we are outside a struct, keep on consuming stdin
	 * until we found the '{' marking as the beginning of
	 * a struct
	 *
	 * we add this because
	 *   1. we can parse more than one node tree in a file
	 *   2. if there are anything else such as LOG: statement
	 *      we can safely ignore then. in the old days, those
	 *      thing have to be removed by hand.
	 * */
	int level = 0;

    while (1)
	{
        int this_option_optind = optind ? optind : 1;
        int option_index = 0;
        static struct option long_options[] =
		{
			/* name      has_arg          flag val*/
            {"help",      no_argument,       0,  0 },
            {"skip-empty",no_argument,       0,  0 },
            {"color",     no_argument,       0,  0 },
            {"skip-node", required_argument, 0,  0 },
            {0,           0,                 0,  0 }
        };

        c = getopt_long(argc, argv, "h", long_options, &option_index);
        
		if (c == -1)
            break;

        switch (c)
		{
           case 0:
                if (strcmp("help", long_options[option_index].name) == 0)
				{
					usage();
					exit(0);
				}

                if (strcmp("skip-empty", long_options[option_index].name) == 0)
				{
					is_skip_empty = 1;
					fprintf(stderr,"%s:skip empty elements\n",progname);
				}

                if (strcmp("color", long_options[option_index].name) == 0)
				{
					is_color = true;
					fprintf(stderr,"%s:color output\n",progname);
				}

                if (strcmp("skip-node", long_options[option_index].name) == 0)
				{
					skip_node_name = strdup(optarg);
					fprintf(stderr,"%s:skip mode named:%s\n",progname,skip_node_name);
				}
                break;

           case 'h':
		   case '?':
				usage();
				exit(0);
           		break;

           default:
                printf("?? getopt returned character code 0%o ??\n", c);

		}
#if 0 
		if (optind < argc)
		{
           printf("non-option ARGV-elements: ");
           while (optind < argc)
               printf("%s ", argv[optind++]);
          printf("\n");
        }
#endif
	}

	/*
     * in previous we alloc stack on stack which raise an link error 
     * relocation truncated to fit: R_X86_64_PC32 against 
     * symbol `stack' defined in COMMON section 
     * to fix this alloc it in heap using malloc
     */
	stack = (int*)malloc(sizeof(int) * LENGTH);

	print_header();
	while (EOF != (c = getc(stdin)))
	{
 		char *name = NULL;
		switch(c)
		{
			case '{': /* a new node*/
				level++;
				parent_node_num = get(node_num);
				parent_elem_num = get(elem_num);

				push(parent_node_num); /* push node_num first*/
				push(parent_elem_num); /* push elem_num second */

				set(elem_num,0); /* the new node elem_num start at 0 */
				
				add_node(new(node_cnt), get_one_name()); /* node_cnt always ++*/
				set(node_num, get(node_cnt)); /* set the node_num */
				child_node_num = get(node_num);

				if (get(node_cnt) != 0)
				{
					add_link(parent_node_num, parent_elem_num, child_node_num);     /* if on sub level add links */
				}
				break;

			case '}': /* end of the node */
				/* we are not in a struct */
				if (level <= 0)
					continue;

				/* restore the parent */
				set(elem_num, pop()); /* pop elem_num first */
				set(node_num, pop()); /* pop node_num second */
				/* we are out if the stack */
				level--;
				break;

			case ':':  /* a new item */
				/* we are not in a struct */
				if (level <= 0)
					continue;

				name = get_one_name();

				if (name != NULL)
					add_item(get(node_num), new(elem_num), name);
				break;

			defalut:
				/* if we are not in struct consume input until we meet one */
				if (level <= 0)
				{
					while(EOF!= (c = getc(stdin)) && c !='{' ) ;
					/* put it back */
					ungetc('{',stdin);
				}
				break;
		}
	}

	print_body();
	print_footer();
}

int print_header(void)
{
	printf("digraph Query {\n");
	printf("size=\"100000,100000\";\n");
	printf("rankdir=LR;\n");
	printf("node [shape=none];\n");
}

int add_node(int num, char *name)
{
	int i,j;
#ifdef DEBUG
	printf("add_node(%s)\n", name);
#endif
	memcpy(nodes[num].name, name, LENGTH);

	if (!is_color)
	{
		nodes[num].color = "black";
	}
	else
	{
		if (strstr(name, "QUERY"))
		{
			nodes[num].color = "red";
		}
		else if (strstr(name, "RTE"))
		{
			nodes[num].color = "blue";
		}
		else if (strstr(name, "TARGETENTRY"))
		{
			nodes[num].color = "orange";
		}
		else if (strstr(name, "RELOPTINFO"))
		{
			nodes[num].color = "green";
		}
		else
		{
			nodes[num].color = "black";
		}
	}

	free(name);
	for (i = 0; i < LENGTH; i++)
	{
		nodes[num].links_count = 0;

		/* clear all the item name */
		for (j = 0; j < LENGTH; j++)
		{
			memset(nodes[num].elems[j].name,0,LENGTH);
		}
	}
}

int print_footer(void)
{
	printf("\n}");
}

int add_item(int node_n, int elem_n, char *name)
{
#ifdef DEBUG
	printf("add_item(node_n=%d, elem_n=%d, name=%s)\n", node_n, elem_n, name);
#endif
	memcpy(nodes[node_n].elems[elem_n].name, name, LENGTH);
	free(name);
}

/*
 * get one node or element name from stdin
 * if name is ok return the name
 * otherwise return NULL
 */
char *get_one_name()
{
	char * buffer = (char*)malloc(LENGTH);
	int i = 0;
	int c = 0;
	int len = 0;
	// bool found_zero = false, found_non_zero = false;
	char *pos;

	memset(buffer, 0, LENGTH);
	while((c = getc(stdin)) != ':' && c != '{' && c != '}')
	{
		buffer[i++] = c;
	}

	/* push back the token */
	ungetc(c, stdin);

	len = strlen(buffer);

	/* remove any illeagal char of dot format */
	for (i = 0; i < len; i++)
	{
		if (buffer[i]=='"')
			buffer[i]=' ';

		if (buffer[i]=='<' ||
            buffer[i]=='>') 
			buffer[i]='-';

		/* if (buffer[i] == '0')
			found_zero = true;

		if (buffer[i] > '1' && buffer[i] < '9')
			found_non_zero = true; */
	}

	if ((pos = strstr(buffer,"(")) && !strstr(buffer,")"))
	{
		pos[0] = ' ';
	}

	/* if (is_skip_empty)
	{
		if (found_zero && !found_non_zero)
		{
			free(buffer);
			return NULL;
		}
	} */

	if (is_skip_empty && strstr(buffer,"false"))
	{
		free(buffer);
		return NULL;
	}

	if (is_skip_empty && strstr(buffer,"--"))
	{
		free(buffer);
		return NULL;
	}

	return buffer;
}

int print_body(void)
{
	int i, j;

	/* print the nodes */
	for (i = 0; i <= get(node_cnt); i++)
	{
		char stri[128] = { 0 };
		char strj[128] = { 0 };
		if (strcmp(nodes[i].name,"") == 0)
			continue;
	
		if (skip_node_name && strstr(nodes[i].name, skip_node_name))
			continue;

		remove_line_breaks(nodes[i].name);
		strcpy(stri, nodes[i].name);
		printf("node%d [label=<<table border=\"0\" cellspacing=\"0\" color=\"%s\">\n"
			   "	<tr>\n"
			   "		<td port=\"f0\" border=\"1\" align=\"center\" valign=\"middle\">\n"
			   "			<b><font face=\"verdana\">%s</font></b>\n"
			   "		</td>\n"
			   "	</tr>\n",
			   i, nodes[i].color, stri);

		for (j = 1; j < LENGTH; j++)
		{
			if (strlen(nodes[i].elems[j].name) != 0)
			{
				remove_line_breaks(nodes[i].elems[j].name);
				strcpy(strj, nodes[i].elems[j].name);
				printf("	<tr><td port=\"f%d\" border=\"1\" align=\"center\" valign=\"middle\"><font face=\"arial\">%s</font></td></tr>\n", 
					   j, strj);
			}
			else
			{
				break;
			}
		}

		printf("	</table>>\n];\n");
	}	

	/* print the links */
	for (i = 0; i < get(node_cnt); i++)
	{
		if (strcmp(nodes[i].name, "") == 0)
			continue;
	
		if (skip_node_name && strstr(nodes[i].name, skip_node_name))
			continue;

		for (j = 0; j < nodes[i].links_count; j++)
			printf("%s", nodes[i].links[j]);
	}
}

int add_link(int parent_node_num, int parent_elem_num, int child_node_num)
{
	int a;
	srand(time(0) + rand());
	a = rand() % (sizeof(linecolor)/sizeof(linecolor[0]));
	sprintf(nodes[parent_node_num].links[nodes[parent_node_num].links_count++], 
             "node%d:f%d -> node%d:f%d[color=\"%s\", style=\"bold\"]\n",
			 parent_node_num,
			 parent_elem_num,
			 child_node_num,
             (int)0, linecolor[a]);
}
