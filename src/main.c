#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <GL/glut.h>

#define N_PROGRAMS 1000
#define MEM_SIZE 2048

uint64_t state64 = 0xDEADBEEFCAFEBABE;

typedef struct {
    uint8_t mem[MEM_SIZE];     // Bytecode instructions
    //uint8_t data[DATA_SIZE];     // Data memory
    uint8_t dp;                  // Data pointer
    uint8_t ip;                  // Instruction pointer
    int halted;
    int running;                 // Live execution toggle
} BFFInterpreter;

BFFInterpreter interpreter;

const char *mnemonics[16] = {
    "NOP", "INC", "DEC", "MOVR", "MOVL", "ADD", "SUB", "JZ",
    "JNZ", "OUT", "IN", "RND", "CLR", "SWP", "CPY", "HALT"
};

uint8_t xorshift64() {
    state64 ^= state64 >> 12;
    state64 ^= state64 << 25;
    state64 ^= state64 >> 27;
    return (uint8_t)((state64 * 268) & 0xFF);
}
void SeedRandom() {
    uint64_t seed = ((uint64_t)rand() << 32) | rand();
    state64 = seed;
}

void FillTapeWithRandomInstructions(BFFInterpreter *interpreter) {
    for (int i = 0; i < MEM_SIZE; i++) {
        uint8_t opcode = xorshift64() % 16;
        uint8_t arg    = xorshift64() % 16;
        interpreter->mem[i] = (opcode << 4) | arg;
    }
}

void RenderBitmapString(float x, float y, void *font, const char *string) {
    glRasterPos2f(x, y);
    for (const char *c = string; *c != '\0'; c++) {
        glutBitmapCharacter(font, *c);
    }
}

void StepInterpreter(BFFInterpreter *interpreter) {
    if (interpreter->halted || interpreter->ip >= MEM_SIZE) return;

    //uint8_t instr = interpreter->mem[interpreter->ip++];
    uint8_t instr = interpreter->mem[interpreter->ip];
    interpreter->ip = (interpreter->ip + 1) % MEM_SIZE;
    
    uint8_t opcode = instr >> 4;
    uint8_t arg = instr & 0x0F;

        switch (opcode) {
            case 0x0: /* NOP */ break;
            case 0x1:  // INC x
                interpreter->mem[arg]++;
                break;
            case 0x2:  // DEC x
                interpreter->mem[arg]--;
                break;
            case 0x3:  // MOVR x
                interpreter->dp = (interpreter->dp + arg) % MEM_SIZE;
                break;
            case 0x4:  // MOVL x
                interpreter->dp = (interpreter->dp - arg + MEM_SIZE) % MEM_SIZE;
                break;
            case 0x5:  // ADD x
                interpreter->mem[interpreter->dp] += interpreter->mem[arg];
                break;
            case 0x6:  // SUB x
                interpreter->mem[interpreter->dp] -= interpreter->mem[arg];
                break;
            case 0x7:  // JZ x
                if (interpreter->mem[interpreter->dp] == 0)
                    interpreter->ip = (interpreter->ip + arg) % MEM_SIZE;
                break;
            case 0x8:  // JNZ x
                if (interpreter->mem[interpreter->dp] != 0)
                    interpreter->ip = (interpreter->ip - arg + MEM_SIZE) % MEM_SIZE;
                break;
            case 0x9:  // OUT x
                putchar(interpreter->mem[arg]);
                break;
            case 0xA:  // IN x
                interpreter->mem[arg] = getchar();
                break;
            case 0xB:  // RND x
                interpreter->mem[arg] = xorshift64() & 0xFF;
                break;
            case 0xC:  // CLR x
                interpreter->mem[arg] = 0;
                break;
            case 0xD:  // SWP x
                {
                    uint8_t tmp = interpreter->mem[arg];
                    interpreter->mem[arg] = interpreter->mem[interpreter->dp];
                    interpreter->mem[interpreter->dp] = tmp;
                }
                break;
            case 0xE:  // CPY x
                interpreter->mem[arg] = interpreter->mem[interpreter->dp];
                break;
            case 0xF:  // HALT
                interpreter->halted = 1;
                break;
            default:
                fprintf(stderr, "Unknown opcode: %x\n", opcode);
                interpreter->halted = 1;
                break;
        }

}

void DrawMemoryRow(const uint8_t *mem, int highlight_index, float y, const char *label) {
    float boxWidth = 1.8f / MEM_SIZE;

    for (int i = 0; i < MEM_SIZE; i++) {
        if (i == highlight_index)
            glColor3f(1.0f, 0.0f, 0.0f); // highlight: red
        else
            glColor3f(0.4f, 0.7f, 1.0f); // normal: blue

        float x = -0.9f + i * boxWidth;

        glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + boxWidth - 0.01f, y);
        glVertex2f(x + boxWidth - 0.01f, y + 0.2f);
        glVertex2f(x, y + 0.2f);
        glEnd();

        glColor3f(0, 0, 0);
        char labelText[6];
        snprintf(labelText, sizeof(labelText), "%02X", mem[i]);
        RenderBitmapString(x + 0.01f, y + 0.07f, GLUT_BITMAP_HELVETICA_12, labelText);
    }

    // Label
    glColor3f(1, 1, 1);
    RenderBitmapString(-0.95f, y + 0.25f, GLUT_BITMAP_HELVETICA_12, label);
}

void display(void) {
    glClear(GL_COLOR_BUFFER_BIT);

    DrawMemoryRow(interpreter.mem, interpreter.ip, 0.4f, "Tape");
    DrawMemoryRow(interpreter.mem, interpreter.dp, 0.1f, "Data");

    glutSwapBuffers();
}

void timer(int value) {
    if (interpreter.running && !interpreter.halted)
        StepInterpreter(&interpreter);

    glutPostRedisplay();
    glutTimerFunc(500, timer, 0); // call every 500ms
}

void keyboard(unsigned char key, int x, int y) {
    if (key == ' ') {
        interpreter.running = !interpreter.running;
    } else if (key == 's') {
        StepInterpreter(&interpreter);
    } else if (key == 27) {
        exit(0); // ESC key
    }
    glutPostRedisplay();
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(-1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char **argv) {
     srand(time(NULL));
     SeedRandom();
    //uint64_t programs[N_PROGRAMS];
    //uint64_t programs[N_PROGRAMS];
    //GenerateRandomData(programs, N_PROGRAMS, time(NULL));

    memset(&interpreter, 0, sizeof(interpreter));
    //GenerateRandomData(interpreter.tape, TAPE_SIZE, time(NULL));
    //GenerateRandomData(interpreter.data, DATA_SIZE, time(NULL));
    
    FillTapeWithRandomInstructions(&interpreter);
    // interpreter.tape[0] = 0x11; // INC 1
     //interpreter.tape[1] = 0x11; // INC 1
    // interpreter.tape[2] = 0x91; // OUT 1
    // interpreter.tape[3] = 0xF0; // HALT

    // GLUT setup
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(1600, 800);
    glutCreateWindow("BFF Interpreter Visualizer");

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(500, timer, 0); // First call in 500ms

    glClearColor(0.1f, 0.1f, 0.1f, 1);

    glutMainLoop();
    return 0;
}
