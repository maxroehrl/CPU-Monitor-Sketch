/*
 *  Author: Max Röhrl
 *  Created: 21.03.2014
 *  Description: Empfängt die CPU Auslastung eines PC und zeigt sie mit einem Schrittmotor an
 */

#include <stdlib.h>        // atoi() abs()
#include <string.h>        // memset()
#include <avr/io.h>        // Atmega IO Definitionen
#include <avr/interrupt.h> // sei()
#include <avr/pgmspace.h>  // PSTR()
#include <util/delay.h>    // _delay_ms()
#include "uart.h"          // UART Funktionen

// Baud Rate hier festlegen
#define UART_BAUD_RATE     9600

// Port und Pins hier ändern
#define STEPPER_PORT       PORTD
#define ENABLE_PIN         2
#define HALFFULL_PIN       3
#define CLOCK_PIN          4
#define CWCWW_PIN          5

// Übersetzung: 360° entspricht 100% Auslastung
#define STEP_FACTOR        4

// Wenn dieser Char empfangen wird, ist der Buffer komplett und repräsentiert eine
// Zahl von 1 bis 100
#define READ_TO_CHAR       33
// Wenn dieser Char empfangen wird, resetet sich der Schrittmotor auf die
// Ausgangsposition
#define RESET_STEPPER_CHAR 35

// Makros
#define SET_HIGH(pin)      STEPPER_PORT |= 1 << pin
#define SET_LOW(pin)       STEPPER_PORT &= ~(1 << pin)
#define SET_OUTPUT(pin)    DDRD |= 1 << pin

// Prädeklarationen
void doSteps(int steps);
void setClockWise(void);
void setCounterClockWise(void);

// Haupteinstiegspunkt
int main(void) {
    // Variablen
    unsigned int input;
    char buffer[4],
         output[5];
    int bufferIndex = 0,
        cpuUsage    = 0,
        oldCpuUsage = 0,
        steps       = 0;

    // UART intialisieren
    uart_init(UART_BAUD_SELECT(UART_BAUD_RATE, F_CPU));

    // UART benötigt Interrupts
    sei();

    // Alle Pins sind Outputs
    SET_OUTPUT(HALFFULL_PIN);
    SET_OUTPUT(CLOCK_PIN);
    SET_OUTPUT(CWCWW_PIN);
    SET_OUTPUT(ENABLE_PIN);

    // Halb Schritt Modus (HIGH)
    SET_HIGH(HALFFULL_PIN);

    // Setze Clock auf HIGH für den Anfang
    SET_HIGH(CLOCK_PIN);

    while(1) {
        // Checke ob Bytes im Ringbuffer sind
        input = uart_getc();

        if (input & UART_NO_DATA) {
            // Nichts tun
        } else if (input == RESET_STEPPER_CHAR) {
            if (input & UART_FRAME_ERROR) uart_puts_P("UART Frame Error: ");
            if (input & UART_OVERRUN_ERROR) uart_puts_P("UART Overrun Error: ");
            if (input & UART_BUFFER_OVERFLOW) uart_puts_P("Buffer Overflow Error: ");

            // Wenn ein '#' gesendet wird, resete den Schrittmotor
            setCounterClockWise();
            doSteps(cpuUsage);
            cpuUsage = 0;
        } else {
            if (input & UART_FRAME_ERROR) uart_puts_P("UART Frame Error: ");
            if (input & UART_OVERRUN_ERROR) uart_puts_P("UART Overrun Error: ");
            if (input & UART_BUFFER_OVERFLOW) uart_puts_P("Buffer Overflow Error: ");

            // Speichere empfangenen Char im Buffer
            buffer[bufferIndex++] = input;

            // READ_TO_CHAR legt das Ende der Zahl fest
            if(input == READ_TO_CHAR) {
                // Der READ_TO_CHAR im letzen Feld wird ignoriert
                buffer[--bufferIndex] = 0;

                // Alte Auslastung wird vermerkt
                oldCpuUsage = cpuUsage;

                // Speichere die empfangene Auslastung als Integer
                cpuUsage = atoi(buffer);

                // Berechne die Anzahl der Schritte die der Motor machen muss
                // alte CPU Auslastung + xSchritte = neue CPU Auslastung
                steps = cpuUsage - oldCpuUsage;

                // Sende die gemachten Schritte zurück
                itoa(steps, output, 10);
                uart_puts(output);
                // Sende den READ_TO_CHAR
                uart_putc('!');

                if(steps > 0) {
                    // x Schritte im Uhrzeigersinn
                    setClockWise();
                    doSteps(steps);
                } else if (steps < 0) {
                    // x Schritte gegen den Uhrzeigersinn
                    setCounterClockWise();
                    doSteps(steps);
                }
                // Wenn es 0 Schritte sind mache nichts

                // Setze Buffer und Index zurück
                memset(&buffer[0], 0, sizeof(buffer));
                bufferIndex = 0;
            }
        }
    }
    return 0;
}

/**********************************************
Gewisse Anzahl an Schritten ausführen
***********************************************/
void doSteps(int steps) {
    // Betrag von steps berechnen und mal den Faktor
    int stepNumber = abs(steps) * STEP_FACTOR;

    for(int i = 0; i < stepNumber; i++) {
        // Der Schritt wird bei steigenden Signal gemacht
        SET_HIGH(ENABLE_PIN);
        SET_LOW(CLOCK_PIN);
        _delay_ms(1);

        SET_HIGH(CLOCK_PIN);
        SET_LOW(ENABLE_PIN);
        _delay_ms(1);
    }
}

/**********************************************
Setze auf Drehung im Uhrzeigersinn (HIGH)
***********************************************/
void setClockWise(void) {
    SET_HIGH(CWCWW_PIN);
    _delay_ms(1);
}

/**********************************************
Setze auf Drehung gegen den Uhrzeigersinn (LOW)
***********************************************/
void setCounterClockWise(void) {
    SET_LOW(CWCWW_PIN);
    _delay_ms(1);
}
