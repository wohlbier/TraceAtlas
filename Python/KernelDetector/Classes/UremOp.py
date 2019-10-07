#!/usr/bin/python
# -*- coding: utf-8 -*-
from Classes.AluOp import AluOp


class UremOp(AluOp):

    def __init__(
        self,
        line,
        block,
        inst,
        ultimaValue,
        archValue,
        ):

        AluOp.__init__(
            self,
            line,
            block,
            inst,
            ultimaValue,
            archValue,
            )
        eqSplit = inst.split('=')
        self.output = eqSplit[0].strip().strip('<').strip('>')
        commaSeparated = eqSplit[1].strip(' ').split(',')
        if len(commaSeparated) == 2:
            spaceSeparated = commaSeparated[0].split(' ')
            if len(spaceSeparated) == 3:
                self.type = spaceSeparated[1]
                self.ultimaValue = spaceSeparated[2]
                self.archValue = commaSeparated[1].strip(' ')
            else:
                raise Exception('Illegally formatted UremOp expression: '
                                 + inst)
        else:
            raise Exception('Illegally formatted UremOp expression: '
                            + inst)