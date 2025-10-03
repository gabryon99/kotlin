/*
 * Copyright 2010-2022 JetBrains s.r.o. and Kotlin Programming Language contributors.
 * Use of this source code is governed by the Apache 2.0 license that can be found in the license/LICENSE.txt file.
 */

package org.jetbrains.kotlin.backend.konan.driver.phases

import org.jetbrains.kotlin.backend.common.phaser.createSimpleNamedCompilerPhase
import org.jetbrains.kotlin.backend.konan.BitcodeCompiler
import org.jetbrains.kotlin.backend.konan.driver.PhaseContext
import java.io.File

internal data class ObjectFilesPhaseInput(
        val bitcodeFile: File,
        val objectFile: File,
)

/**
 * Compile the input LLVM bitcode file into an Object File containing assembly instructions of target architecture.
 * The bitcode is compiled using `clang++`.
 */
internal val ObjectFilesPhase = createSimpleNamedCompilerPhase<PhaseContext, ObjectFilesPhaseInput>(
        name = "ObjectFiles",
) { context, input ->
    BitcodeCompiler(context).makeObjectFile(input.bitcodeFile, input.objectFile)
}
