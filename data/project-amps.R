#!/usr/local/bin/Rscript --vanilla


# ------------------------------ #
rm(list=ls())

options(echo=TRUE)
options(width=80)
options(warn=2)
options(scipen=10)
options(datatable.prettyprint.char=50)
options(datatable.print.class=TRUE)
options(datatable.print.keys=TRUE)
options(datatable.fwrite.sep='\t')
options(datatable.na.strings="")
options(mc.cores = parallel::detectCores())
# rstan_options(auto_write = TRUE)

args <- commandArgs(trailingOnly=TRUE)

library(colorout)
library(data.table)
library(magrittr)
library(stringr)
library(libbib)

source("~/.rix/tony-utils.R")
# ------------------------------ #


dat <- fread("./amp-measurements.csv")

dat %>% melt(measure.vars=c("b1", "b2", "b3", "b4"),
             variable.name="brightness",
             value.name="milliamps") -> dat

dat[, brightness:=as.factor(str_sub(brightness, 2, 2))]


library(ggplot2)

dat[pattern==1] %>%
  ggplot(aes(x=brightness, y=milliamps, group=num_sticks)) +
  geom_line() + ylim(0, 300)

# probably around 240


dat[pattern==2]
dat[pattern==2] %>%
  ggplot(aes(x=brightness, y=milliamps, group=num_sticks)) +
  geom_line() +
  facet_grid(rows = vars(color))

dat[pattern==2] %>%
  ggplot(aes(x=brightness, y=milliamps, group=color, color=color)) +
  geom_line() +
  facet_grid(rows = vars(num_sticks))

dat[pattern==2] %>%
  ggplot(aes(x=brightness, y=milliamps, colour=color, shape=num_sticks,
             group=interaction(num_sticks,color))) +
  geom_line() + geom_point()

# no more than 350
# probably around 310 (for full warm white, that is)

