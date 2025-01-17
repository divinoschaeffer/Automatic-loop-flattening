/**
 * @author SORGHO Nongma
 * @brief Edits an OpenSCoP representation to generate an output code where loops are collapsed.
 * @version 0.9
 * @date 2024-02-09
 * @details The code generation is done in three steps:
 * 1. Generate the code for each boundary
 * 2. Generate the header file for each boundary
 * 3. Merge the generated code with the original code
 * @copyright Copyright (c) 2024
 */

#include "codegen.h"

FILE *fs;
extern TCD_FlowData *tcdFlowData;

unsigned boundary_index = 0;

/**
 * @brief Writes the initialisation section of the generated code.
 * @param boundary
 */
void write_init_section(TCD_Boundary boundary)
{
    char *outer_var_bounds = boundary->outerLoopUpperBound;
    char *iteration_domains = boundary->iterationDomainsString;
    char **name_array = boundary->nameArray;

    // include trahrhe header
    fs_writef("#include \"%s.h\"\n", tcdFlowData->outputFile);

    fs_writef("//start//");

    fs_writef("unsigned pc_%d;", boundary_index);
    // we need to index ehrhart calls as they may be outer vars with the same name among different boundaries
    fs_writef("unsigned upper_bound_%d = Ehrhart%d(%s);", boundary_index, boundary_index, outer_var_bounds);
    fs_writef("unsigned first_iteration_%d = 1;", boundary_index);
    fs_writef("#pragma omp parallel for private(%s) firstprivate(first_iteration_%d) schedule(static)", iteration_domains, boundary_index);
    fs_writef("for (pc_%d = 1; pc_%d <= upper_bound_%d; pc_%d++)", boundary_index, boundary_index, boundary_index, boundary_index);
    fs_writef("{");

    fs_tabular();

    fs_writef("if (first_iteration_%d)", boundary_index);
    fs_writef("{");

    fs_tabular();

    int max_depth = tcdFlowData->collapseParameters[boundary_index];
    int curr_depth = 0;
    while (curr_depth < max_depth)
    {
        // Construct variables on which the iteration variable depends
        char *vars = (char *)malloc(1024 * sizeof(char));

        sprintf(vars, "pc_%d", boundary_index);

        // add iteration variables from start to curr_depth
        for (int i = 0; i < curr_depth; i++)
        {
            strcat(vars, ",");
            strcat(vars, name_array[i + 1]);
        }

        if (outer_var_bounds != NULL && outer_var_bounds[0] != '\0')
        {
            strcat(vars, ",");
            strcat(vars, outer_var_bounds);
        }

        fs_writef("%s = trahrhe_%s%d(%s);", name_array[curr_depth + 1], name_array[curr_depth + 1], boundary_index, vars);

        curr_depth++;
    }
    fs_writef("first_iteration_%d = 0;", boundary_index);
    fs_untabular();
    fs_writef("}");
    fs_writef("");
    fs_untabular();
}

/**
 * @brief Increments the loop variables
 * @param curr_depth
 * @param outer_var_bounds
 * @param name_array
 * @param stop_conditions
 * @param stop_conditions_int
 * @param initial_values
 * @param options
 * @return
 */
void increment(int curr_depth,
               char *outer_var_bounds,
               char **name_array,
               struct clast_expr *stop_conditions[],
               int *stop_conditions_int,
               struct clast_expr *initial_values[],
               CloogOptions *options)
{
    if (curr_depth == 0)
    {
        return;
    }
    // the innermost loop is incremented first, then when it reaches its upper bound, the next loop is incremented  etc.
    char *tmp = (char *)malloc(1024 * sizeof(char));
    FILE *tmpFile = fopen("tmp2.c", "w+");
    if (tmpFile == NULL)
    {
        fprintf(stderr, "Error: Unable to open file tmp.c\n");
        exit(EXIT_FAILURE);
    }
    clast_pprint_expr(options, tmpFile, stop_conditions[curr_depth]);
    fseek(tmpFile, 0, SEEK_END);
    long fsize = ftell(tmpFile);
    fseek(tmpFile, 0, SEEK_SET);
    char *string = (char *)malloc(fsize + 1);
    fread(string, 1, fsize, tmpFile);
    fclose(tmpFile);
    string[fsize] = 0;
    tabString(tmpFile, string, fsize);
    if (stop_conditions_int[curr_depth - 1] == 0)
    {
        sprintf(tmp, "if (%s > %s)", name_array[curr_depth + 1], string);
    }
    else
    {
        sprintf(tmp, "if (%s < %s)", name_array[curr_depth + 1], string);
    }

    // print
    fs_writef("%s", tmp);
    fs_writef("{");

    fs_tabular();

    fs_writef("%s++;", name_array[curr_depth]);
    // increment the next loop - initial value
    tmpFile = fopen("tmp3.c", "w+");
    if (tmpFile == NULL)
    {
        fprintf(stderr, "Error: Unable to open file tmp.c\n");
        exit(EXIT_FAILURE);
    }

    clast_pprint_expr(options, tmpFile, initial_values[curr_depth]);
    fseek(tmpFile, 0, SEEK_END);
    fsize = ftell(tmpFile);
    fseek(tmpFile, 0, SEEK_SET);
    string = (char *)malloc(fsize + 1);
    fread(string, 1, fsize, tmpFile);
    fclose(tmpFile);
    string[fsize] = 0;
    tabString(tmpFile, string, fsize);

    increment(curr_depth - 1, outer_var_bounds, name_array, stop_conditions, stop_conditions_int, initial_values, options);

    fs_writef("%s = %s;", name_array[curr_depth + 1], string);

    fs_untabular();
    fs_writef("}");

    remove("tmp2.c");
    remove("tmp3.c");
}

/**
 * @brief Writes the increment section of the generated code
 * @details The increment section is the part of the code that increments the loop variables
 * @param boundary
 * @param stop_conditions
 * @param stop_conditions_int
 * @param initial_values
 * @param options
 */
void write_increment_section(TCD_Boundary boundary, struct clast_expr *stop_conditions[], int *stop_conditions_int, struct clast_expr *initial_values[], CloogOptions *options)
{
    char *outer_var_bounds = boundary->outerLoopUpperBound;
    char **name_array = boundary->nameArray;

    int max_depth = tcdFlowData->collapseParameters[boundary_index];
    fs_tabular();
    fs_writef("%s++;", name_array[max_depth]);
    increment(max_depth - 1, outer_var_bounds, name_array, stop_conditions, stop_conditions_int, initial_values, options);
    fs_untabular();
}

/**
 * @brief  Generates the code segment for a given boundary
 * @param root
 * @param options
 * @param boundary
 */
void generateCodeSegment(struct clast_stmt *root, CloogOptions *options, TCD_Boundary boundary)
{
    // Initialisation code
    write_init_section(boundary);

    // Insert the actual unchanged code
    FILE *tmp = fopen("tmp.c", "w+");

    if (tmp == NULL)
    {
        fprintf(stderr, "Error: Unable to open file tmp.c\n");
        exit(EXIT_FAILURE);
    }

    int loop_nest_depth = tcdFlowData->collapseParameters[boundary_index];
    int max_depth = loop_nest_depth;
    struct clast_expr *stop_conditions[loop_nest_depth];
    struct clast_expr *initial_values[loop_nest_depth];
    int *stop_conditions_int = (int *)malloc(loop_nest_depth * sizeof(int));
    while (loop_nest_depth > 0)
    {
        if (CLAST_STMT_IS_A(root, stmt_root))
        {
            struct clast_root *root_stmt = (struct clast_root *)root;
            // equation, n, then
            root = root_stmt->stmt.next;
        }

        else if (CLAST_STMT_IS_A(root, stmt_for))
        {
            struct clast_for *for_stmt = (struct clast_for *)root;
            initial_values[max_depth - loop_nest_depth] = for_stmt->LB;
            root = for_stmt->body;
            if (for_stmt->UB != NULL)
            {
                stop_conditions[max_depth - loop_nest_depth] = for_stmt->UB;
                stop_conditions_int[max_depth - loop_nest_depth] = 0;
            }
            else
            {
                stop_conditions[max_depth - loop_nest_depth] = for_stmt->LB;
                stop_conditions_int[max_depth - loop_nest_depth] = 1;
            }
            loop_nest_depth--;
        }
        else if (CLAST_STMT_IS_A(root, stmt_block))
        {
            struct clast_block *block_stmt = (struct clast_block *)root;
            root = block_stmt->body;
        }
        else if (CLAST_STMT_IS_A(root, stmt_user))
        {
            struct clast_user_stmt *user_stmt = (struct clast_user_stmt *)root;
            root = user_stmt->substitutions;
        }
        else if (CLAST_STMT_IS_A(root, stmt_guard))
        {
            struct clast_guard *guard_stmt = (struct clast_guard *)root;
            root = guard_stmt->then;
        }
        else if (CLAST_STMT_IS_A(root, stmt_ass))
        {
            struct clast_assignment *assign = (struct clast_assignment *)root;
            root = assign->stmt.next;
        }
        else
        {
            fprintf(stderr, "Error: Unable to find the loop nest depth\n");
            exit(EXIT_FAILURE);
        }
    }

    // Write the output
    clast_pprint(tmp, root, 0, options);

    fseek(tmp, 0, SEEK_END);
    long fsize = ftell(tmp);
    fseek(tmp, 0, SEEK_SET);

    char *string = (char *)malloc(fsize + 1);
    fread(string, 1, fsize, tmp);
    fclose(tmp);

    string[fsize] = 0;

    fs_writef("%s", tabStringReturn(string, fsize));

    // Increment
    write_increment_section(boundary, stop_conditions, stop_conditions_int, initial_values, options);

    // Finalisation code
    fs_writef("}\n");

    fs_writef("//end//\n");
}

/**
 * @brief  Generates the output code where loops are collapsed
 * @param boundaryList
 */
void generateCode(TCD_BoundaryList boundaryList)
{
    osl_scop_p scop = tcdFlowData->scop;

    CloogState *state;
    CloogInput *input;
    CloogOptions *options;
    struct clast_stmt *root;

    state = cloog_isl_state_malloc(isl_ctx_alloc());
    options = cloog_options_malloc(state);
    options->openscop = 1;
    options->scop = scop;
    options->compilable = 1;

    char *outputFilename = (char *)malloc(1024 * sizeof(char));
    strcpy(outputFilename, INTERMEDIATE_FILENAME);
    strcat(outputFilename, ".c");

    fs_open(outputFilename);
    fs_tabular();

    // generation
    TCD_Boundary boundary = boundaryList->first;
    while (boundary != NULL)
    {
        input = cloog_input_from_osl_scop(state, scop);

        if (input == NULL)
        {
            fprintf(stderr, "Error: Unable to generate input from scop\n");
            exit(EXIT_FAILURE);
        }

        root = cloog_clast_create_from_input(input, options);

        if (root == NULL)
        {
            fprintf(stderr, "Error: Unable to generate clast from input\n");
            exit(EXIT_FAILURE);
        }

        generateCodeSegment(root, options, boundary);

        // Next boundary
        boundary = boundary->next;
        scop = scop->next;
        boundary_index++;
    }
    fs_untabular();
    fs_close();
}

/**
 * @brief Generates the header file for a given boundary
 * @param boundary
 * @param outputFile
 * @param boundary_index
 */
void generateBoundaryHeader(TCD_Boundary boundary, FILE *outputFile, int boundary_index)
{
    char *isl_domain = boundary->firstIterationDomainOfUnion->first->iterationDomain;

    printf("isl_domain: %s\n", isl_domain);

    char *bash_command = (char *)malloc(1024 * sizeof(char));
    char *trahrhe_install_directory = (char *)malloc(1024 * sizeof(char));
    if (getenv("TRAHRHE_INSTALL_DIR") == NULL)
    {
        const char *trahrhe_warning_message = "Warning: TRAHRHE_INSTALL_DIR not defined.\n"
                                              "Using default value path: ./trahrhe assuming trahrhe is\n"
                                              "in the same directory as the compiler.\n"
                                              "To set the path, please define the environment variable TRAHRHE_INSTALL_DIR\n"
                                              "with the path to the trahrhe directory.\n"
                                              "Example: export TRAHRHE_INSTALL_DIR=/path/to/trahrhe\n";
        fprintf(stderr, "%s", trahrhe_warning_message);
        strcpy(trahrhe_install_directory, "./trahrhe");
    }
    else
    {
        strcpy(trahrhe_install_directory, getenv("TRAHRHE_INSTALL_DIR"));
    }

    sprintf(bash_command, "cd %s && ./trahrhe -d\"%s\" -s\"%d\" -e", trahrhe_install_directory, isl_domain, boundary_index);
    FILE *tmp = fopen("tmp.sh", "w+");
    if (tmp == NULL)
    {
        fprintf(stderr, "Error: Unable to open file tmp.sh\n");
        exit(EXIT_FAILURE);
    }
    fprintf(tmp, "%s", bash_command);
    fclose(tmp);
    system("chmod +x tmp.sh && ./tmp.sh");
    remove("tmp.sh");

    char *headerFilename = (char *)malloc(1024 * sizeof(char));
    sprintf(headerFilename, "%s/trahrhe_header.h", trahrhe_install_directory);

    FILE *headerFile = fopen(headerFilename, "r");
    if (headerFile == NULL)
    {
        fprintf(stderr, "Error: Unable to open file %s\n", headerFilename);
        exit(EXIT_FAILURE);
    }

    fseek(headerFile, 0, SEEK_END);
    long fsize = ftell(headerFile);
    fseek(headerFile, 0, SEEK_SET);

    char *string = (char *)malloc(fsize + 1);
    fread(string, 1, fsize, headerFile);
    fclose(headerFile);

    string[fsize] = 0;

    fprintf(outputFile, "%s", string);

    remove(headerFilename);

    free(trahrhe_install_directory);
    free(bash_command);
}

/**
 * @brief
 * @param boundaryList
 */
void generateHeaderFile(TCD_BoundaryList boundaryList)
{
    char *headerFilename = (char *)malloc(1024 * sizeof(char));
    strcpy(headerFilename, tcdFlowData->outputFile);
    headerFilename[strlen(headerFilename) - 2] = '\0';
    strcat(headerFilename, ".h");
    FILE *outputFile = fopen(headerFilename, "w+");
    if (outputFile == NULL)
    {
        fprintf(stderr, "Error: Unable to open file %s\n", headerFilename);
        exit(EXIT_FAILURE);
    }

    TCD_Boundary boundary = boundaryList->first;
    int index = 0;
    while (boundary != NULL)
    {
        generateBoundaryHeader(boundary, outputFile, index);

        boundary = boundary->next;
        index++;
    }
}

/**
 * @brief Merges the generated code with the original code
 * @details Uses a shell script
 */
void mergeGeneratedCode()
{
    char *command = (char *)malloc(1024 * sizeof(char));
    char *outputFilename = (char *)malloc(1024 * sizeof(char));

    // take only the name not the path
    strcpy(outputFilename, tcdFlowData->outputFile);
    char *token = strtok(outputFilename, "/");
    while (token != NULL)
    {
        strcpy(outputFilename, token);
        token = strtok(NULL, "/");
    }
    outputFilename[strlen(outputFilename) - 2] = '\0';

    sprintf(command, "bash trahrhe_fusion.sh %s %s.c '#include \"%s.h\"' %s", tcdFlowData->entryFile, INTERMEDIATE_FILENAME, outputFilename, tcdFlowData->outputFile);

    system(command);
}

/**
 * @brief  Removes the temporary files
 */
void removeTemporaryFiles()
{
    char *command = (char *)malloc(1024 * sizeof(char));
    sprintf(command, "rm %s.c %s %s %s", INTERMEDIATE_FILENAME, SCOPED_FILENAME, COLLAPSE_PARAMETERS_FILENAME, "tmp.c");
    system(command);
}