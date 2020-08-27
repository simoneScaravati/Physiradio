#!/usr/bin/env Rscript

suppressMessages(library(data.table))
suppressMessages(library(lubridate))
suppressMessages(library(dplyr))
suppressMessages(library(zoo))

options(max.print = 100)

# "SESSIONE","VERSIONE_MAPPING","ETA","SESSO_M_F","CITTA_RESIDENZA","OCCUPAZIONE","TITOLO_STUDIO","ASCOLTATORE_ABITUALE_SI_NO","DEVICE_FRUIZIONE","CONTESTO","GENERI","DATI_PERCEPITO_1","DATI_PERCEPITO_2","DATI_PERCEPITO_3","DATI_PERCEPITO_4","DATI_PERCEPITO_5","DATI_PERCEPITO_6","DATI_PERCEPITO_7","DATI_PERCEPITO_8","DATI_PERCEPITO_9","DATI_PERCEPITO_10","NUM_CORRISPONDENZE_1_7","SUGGERIMENTI_ASSOCIAZIONI","EFFICACIA","TIPO_DATO_SUGGERITO","INCURIOSITO","COMMENTO_LIBERO"

CSV="dati_questionario.csv"
GRAFICO="stats.jpg"

dati <- read.csv(CSV)
#summary(dati)
print("summary ETA")
summary(dati$ETA)
sd(dati$ETA)

print("summary EFFICACIA")
summary(dati$EFFICACIA)
sd(dati$EFFICACIA)

print("summary INCURIOSITO")
summary(dati$INCURIOSITO)
sd(dati$INCURIOSITO)

# correlazioni (prove)
print("correl ETA INCURIOSITO")
cor(dati$ETA,dati$INCURIOSITO) # quasi 0

print("correl ETA EFFICACIA")
cor(dati$ETA,dati$EFFICACIA) # quasi 0

print("correl INCURIOSITO EFFICACIA")
cor(dati$INCURIOSITO,dati$EFFICACIA) # non sembra male

print("covar INCURIOSITO EFFICACIA")
cov(dati$INCURIOSITO,dati$EFFICACIA) # positiva

jpeg(GRAFICO, width = 30, height = 15, units = "cm", res = 300)
par(mfrow = c(2, 3),las=3) # xaxis label verticale

hist(dati$ETA)

barplot(table(dati$ASCOLTATORE_ABITUALE_SI_NO))
#hist(dati$NUM_CORRISPONDENZE_1_7)

hist(dati$EFFICACIA)
hist(dati$INCURIOSITO)

plot(dati$EFFICACIA,dati$INCURIOSITO)
