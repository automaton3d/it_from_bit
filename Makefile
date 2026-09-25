# ================================================
# Makefile for Automaton - CPU ou CUDA
# Use: nmake                     (CPU-only)
#      nmake USE_CUDA=1          (CUDA acelerado)
# ================================================

TARGET = automaton.exe
BUILD_DIR = build
OBJ_DIR = obj

!IFDEF USE_CUDA
ENABLE_CUDA = 1
!ELSE
ENABLE_CUDA = 0
!ENDIF

# ================================================
# Configuracao de candidatos (RESEARCH_LOG.md: "Decision pack") -- PROMOVIDA em 2026-09-24
#   nmake                 -> build promovida (DEFAULT desde 2026-09-24): dispersao de carga +
#                            seed polar + as duas regras de par (CHARGE_DISPERSION_FSM,
#                            POLAR_SEED_FROM_PLACEMENT, PAIR_SAME_OCTANT,
#                            PAIR_OWN_AXIS_EXCHANGE)
#   nmake REFERENCE=1     -> a build de referencia: nenhum macro, e o que o paper cita.  A flag
#                            existia antes da promocao justamente para isto: a referencia
#                            continua buildavel por nome, em vez de por ausencia de flag, e com
#                            a sua propria arvore de objectos (obj_reference\).
#   nmake CANDIDATES=1    -> alias de compatibilidade: constroi o mesmo que o default (a flag
#                            antiga, mantida para os documentos e scripts que a citam).
# Os nomes de saida sao os mesmos (build\automaton.exe, build\first_era_trace.exe), porque
# promover e exatamente substituir o default; objectos vao para obj\ (default/promovida) ou
# obj_reference\ (referencia), para que uma configuracao nunca reutilize objectos da outra
# (nmake nao segue flags).
# ================================================

!IFDEF REFERENCE
!IFDEF CANDIDATES
!ERROR REFERENCE=1 e CANDIDATES=1 sao mutuamente exclusivos: escolha um dos dois.
!ENDIF
!ENDIF

!IFDEF REFERENCE
CANDIDATE_FLAGS =
OBJ_DIR = obj_reference
!MESSAGE [config] referencia (REFERENCE=1): nenhum macro -- a que o paper cita
!ELSE
CANDIDATE_FLAGS = /D "CHARGE_DISPERSION_FSM" /D "POLAR_SEED_FROM_PLACEMENT" /D "PAIR_SAME_OCTANT" /D "PAIR_OWN_AXIS_EXCHANGE"
OBJ_DIR = obj
!MESSAGE [config] promovida (default): CHARGE_DISPERSION_FSM POLAR_SEED_FROM_PLACEMENT PAIR_SAME_OCTANT PAIR_OWN_AXIS_EXCHANGE
!ENDIF

# ================================================
# Objetos comuns
# ================================================

OBJ_COMMON = \
	$(OBJ_DIR)\glad.obj \
	$(OBJ_DIR)\tinyfiledialogs.obj \
	$(OBJ_DIR)\button.obj \
	$(OBJ_DIR)\callback.obj \
	$(OBJ_DIR)\camera.obj \
	$(OBJ_DIR)\core.obj \
	$(OBJ_DIR)\cortina.obj \
	$(OBJ_DIR)\draw_utils.obj \
	$(OBJ_DIR)\globals.obj \
	$(OBJ_DIR)\GUI.obj \
	$(OBJ_DIR)\GUI_2D.obj \
	$(OBJ_DIR)\GUI_3D.obj \
	$(OBJ_DIR)\GUI_Init.obj \
	$(OBJ_DIR)\GUI_Panels.obj \
	$(OBJ_DIR)\help.obj \
	$(OBJ_DIR)\hud.obj \
	$(OBJ_DIR)\input.obj \
	$(OBJ_DIR)\logo.obj \
	$(OBJ_DIR)\main.obj \
	$(OBJ_DIR)\menubar.obj \
	$(OBJ_DIR)\progress.obj \
	$(OBJ_DIR)\projection.obj \
	$(OBJ_DIR)\projection_manager.obj \
	$(OBJ_DIR)\radio.obj \
	$(OBJ_DIR)\recorder.obj \
	$(OBJ_DIR)\replay.obj \
	$(OBJ_DIR)\replay_progress.obj \
	$(OBJ_DIR)\scene.obj \
	$(OBJ_DIR)\shader.obj \
	$(OBJ_DIR)\sound.obj \
	$(OBJ_DIR)\splash.obj \
	$(OBJ_DIR)\subregion_box.obj \
	$(OBJ_DIR)\subregion_modal.obj \
	$(OBJ_DIR)\stats.obj \
	$(OBJ_DIR)\stb_impl.obj \
	$(OBJ_DIR)\text_renderer.obj \
	$(OBJ_DIR)\tickbox.obj \
	$(OBJ_DIR)\tomography.obj \
	$(OBJ_DIR)\initSim.obj \
	$(OBJ_DIR)\interaction.obj \
	$(OBJ_DIR)\simulation.obj \
	$(OBJ_DIR)\utils.obj \
	$(OBJ_DIR)\config.obj \
	$(OBJ_DIR)\render_pipeline.obj \
	$(OBJ_DIR)\Renderer2D.obj \
	$(OBJ_DIR)\geometry.obj \
	$(OBJ_DIR)\polarization.obj \
	$(OBJ_DIR)\charges.obj

OBJ = $(OBJ_COMMON) \
      $(OBJ_DIR)\bridge.obj

!IF $(ENABLE_CUDA)

OBJ = $(OBJ) \
      $(OBJ_DIR)\bridge_cuda.obj \
      $(OBJ_DIR)\cuda_constants.obj \
      $(OBJ_DIR)\cuda_automaton.obj

EXTRA_CPPFLAGS = /D "USE_CUDA" /D "CUDA_BRIDGE_CU"
EXTRA_NVCCFLAGS = -D "USE_CUDA"

!ELSE

OBJ = $(OBJ_COMMON) \
	  $(OBJ_DIR)\bridge.obj

EXTRA_CPPFLAGS =
EXTRA_NVCCFLAGS =

!ENDIF

# ================================================
# Compiladores
# ================================================

CC = cl
NVCC = nvcc

!IFNDEF VCPKG_ROOT
VCPKG_ROOT = E:/vcpkg/installed/x64-windows
!ENDIF
# Careful: a VCPKG_ROOT coming from the environment WINS over the default above, and the vcpkg shipped
# inside Visual Studio (VC\vcpkg) has no freetype, so it links with
#   LINK : fatal error LNK1181: cannot open input file 'freetype.lib'
# Fix it on the command line, which overrides the environment as well:
#   nmake VCPKG_ROOT=E:/vcpkg/installed/x64-windows
# (A developer prompt sets VCPKG_ROOT to the bundled tree; a plain shell may not set it at all, in
# which case the default above is used and the build works without any argument.)

!IFNDEF CUDA_PATH
CUDA_PATH = C:\PROGRA~1\NVIDIA~2\CUDA\v13.2
!ENDIF
CUDA_LIB = "$(CUDA_PATH)/lib/x64"

INCLUDES_MSVC = \
	/I"src\include" \
	/I"src\include\zlib" \
	/I"src" \
	/I"$(VCPKG_ROOT)/include" \
	/I"$(VCPKG_ROOT)/include/freetype2"

INCLUDES_NVCC = \
	-I"src\include" \
	-I"src\include\zlib" \
	-I"src" \
	-I"$(VCPKG_ROOT)/include" \
	-I"$(VCPKG_ROOT)/include/freetype2" \
	-I"src\include\zlib\cuda" \
	-I"src\cuda"

CFLAGS = /nologo /std:c++20 /O2 /EHsc /MD /D "NOMINMAX" $(INCLUDES_MSVC) $(EXTRA_CPPFLAGS) $(CANDIDATE_FLAGS)

NVCC_FLAGS = -allow-unsupported-compiler -c -std=c++17 -O2 -DNOMINMAX -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH=1 $(INCLUDES_NVCC) $(EXTRA_NVCCFLAGS) --compiler-options /MD,/D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH=1

!IF $(ENABLE_CUDA)

LDFLAGS = /link \
	/LIBPATH:"lib" \
	/LIBPATH:"$(VCPKG_ROOT)/lib" \
	/LIBPATH:$(CUDA_LIB) \
	opengl32.lib \
	glfw3dll.lib \
	freetype.lib \
	brotlidec.lib \
	brotlicommon.lib \
	bz2.lib \
	zlib.lib \
	user32.lib \
	gdi32.lib \
	shell32.lib \
	kernel32.lib \
	ole32.lib \
	comdlg32.lib \
	cudart.lib

!ELSE

LDFLAGS = /link \
	/LIBPATH:"lib" \
	/LIBPATH:"$(VCPKG_ROOT)/lib" \
	opengl32.lib \
	glfw3dll.lib \
	freetype.lib \
	brotlidec.lib \
	brotlicommon.lib \
	bz2.lib \
	zlib.lib \
	user32.lib \
	gdi32.lib \
	shell32.lib \
	kernel32.lib \
	ole32.lib \
	comdlg32.lib

!ENDIF

# ================================================
# Targets principais
# ================================================

all: dirs $(BUILD_DIR)\$(TARGET) dlls assets copy_config

dirs:
	if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"
	if not exist "$(OBJ_DIR)" mkdir "$(OBJ_DIR)"

$(BUILD_DIR)\$(TARGET): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) /Fe:$(BUILD_DIR)\$(TARGET) /out:$(BUILD_DIR)\$(TARGET)

# ================================================
# CUDA
# ================================================

!IF $(ENABLE_CUDA)

$(OBJ_DIR)\bridge_cuda.obj: src\cuda\bridge_cuda.cu src\include\zlib\cuda\cuda_sim_optimized.h
	$(NVCC) $(NVCC_FLAGS) -o $@ src\cuda\bridge_cuda.cu

$(OBJ_DIR)\cuda_automaton.obj: src\cuda\cuda_automaton.cu src\include\zlib\cuda\cuda_sim_optimized.h
	$(NVCC) $(NVCC_FLAGS) -o $@ src\cuda\cuda_automaton.cu

$(OBJ_DIR)\cuda_constants.obj: src\cuda\cuda_constants.cu
	$(NVCC) $(NVCC_FLAGS) -o $@ src\cuda\cuda_constants.cu

!ENDIF

# ================================================
# Regras explícitas (Com dependências de cabeçalho)
# ================================================

# --- GUI & Core ---
$(OBJ_DIR)\button.obj: src\button.cpp src\include\button.h
	$(CC) $(CFLAGS) /c src\button.cpp /Fo$(OBJ_DIR)\button.obj

$(OBJ_DIR)\config.obj: src\config.cpp src\include\config.h
	$(CC) $(CFLAGS) /c src\config.cpp /Fo$(OBJ_DIR)\config.obj

$(OBJ_DIR)\callback.obj: src\callback.cpp src\include\callbacks.h
	$(CC) $(CFLAGS) /c src\callback.cpp /Fo$(OBJ_DIR)\callback.obj

$(OBJ_DIR)\camera.obj: src\camera.cpp src\include\camera.h
	$(CC) $(CFLAGS) /c src\camera.cpp /Fo$(OBJ_DIR)\camera.obj

$(OBJ_DIR)\core.obj: src\core.cpp src\include\core.h
	$(CC) $(CFLAGS) /c src\core.cpp /Fo$(OBJ_DIR)\core.obj

$(OBJ_DIR)\cortina.obj: src\cortina.cpp src\include\cortina.h
	$(CC) $(CFLAGS) /c src\cortina.cpp /Fo$(OBJ_DIR)\cortina.obj

$(OBJ_DIR)\draw_utils.obj: src\draw_utils.cpp src\include\draw_utils.h
	$(CC) $(CFLAGS) /c src\draw_utils.cpp /Fo$(OBJ_DIR)\draw_utils.obj

# (the old Dropdown widget was removed: the setup screen uses Cortina only,
#  which is now the single combo box in the project)

$(OBJ_DIR)\globals.obj: src\globals.cpp src\include\globals.h
	$(CC) $(CFLAGS) /c src\globals.cpp /Fo$(OBJ_DIR)\globals.obj

$(OBJ_DIR)\GUI.obj: src\GUI.cpp src\include\GUI.h
	$(CC) $(CFLAGS) /c src\GUI.cpp /Fo$(OBJ_DIR)\GUI.obj

$(OBJ_DIR)\GUI_2D.obj: src\GUI_2D.cpp src\include\GUI.h
	$(CC) $(CFLAGS) /c src\GUI_2D.cpp /Fo$(OBJ_DIR)\GUI_2D.obj

$(OBJ_DIR)\GUI_3D.obj: src\GUI_3D.cpp src\include\GUI.h src\include\voxel.h
	$(CC) $(CFLAGS) /c src\GUI_3D.cpp /Fo$(OBJ_DIR)\GUI_3D.obj

$(OBJ_DIR)\GUI_Init.obj: src\GUI_Init.cpp src\include\GUI.h
	$(CC) $(CFLAGS) /c src\GUI_Init.cpp /Fo$(OBJ_DIR)\GUI_Init.obj

$(OBJ_DIR)\GUI_Panels.obj: src\GUI_Panels.cpp src\include\GUI.h
	$(CC) $(CFLAGS) /c src\GUI_Panels.cpp /Fo$(OBJ_DIR)\GUI_Panels.obj

$(OBJ_DIR)\help.obj: src\help.cpp src\include\help.h
	$(CC) $(CFLAGS) /c src\help.cpp /Fo$(OBJ_DIR)\help.obj

$(OBJ_DIR)\hud.obj: src\hud.cpp src\include\hud.h
	$(CC) $(CFLAGS) /c src\hud.cpp /Fo$(OBJ_DIR)\hud.obj

$(OBJ_DIR)\input.obj: src\input.cpp src\include\input.h
	$(CC) $(CFLAGS) /c src\input.cpp /Fo$(OBJ_DIR)\input.obj

$(OBJ_DIR)\logo.obj: src\logo.cpp src\include\logo.h
	$(CC) $(CFLAGS) /c src\logo.cpp /Fo$(OBJ_DIR)\logo.obj

$(OBJ_DIR)\main.obj: src\main.cpp src\include\GUI.h src\include\model\simulation.h
	$(CC) $(CFLAGS) /c src\main.cpp /Fo$(OBJ_DIR)\main.obj

$(OBJ_DIR)\menubar.obj: src\menubar.cpp src\include\menubar.h
	$(CC) $(CFLAGS) /c src\menubar.cpp /Fo$(OBJ_DIR)\menubar.obj

$(OBJ_DIR)\progress.obj: src\progress.cpp src\include\progress.h
	$(CC) $(CFLAGS) /c src\progress.cpp /Fo$(OBJ_DIR)\progress.obj

$(OBJ_DIR)\projection.obj: src\projection.cpp src\include\projection.h
	$(CC) $(CFLAGS) /c src\projection.cpp /Fo$(OBJ_DIR)\projection.obj

$(OBJ_DIR)\projection_manager.obj: src\projection_manager.cpp src\include\projection_manager.h
	$(CC) $(CFLAGS) /c src\projection_manager.cpp /Fo$(OBJ_DIR)\projection_manager.obj

$(OBJ_DIR)\radio.obj: src\radio.cpp src\include\radio.h
	$(CC) $(CFLAGS) /c src\radio.cpp /Fo$(OBJ_DIR)\radio.obj

$(OBJ_DIR)\recorder.obj: src\recorder.cpp src\include\recorder.h
	$(CC) $(CFLAGS) /c src\recorder.cpp /Fo$(OBJ_DIR)\recorder.obj

$(OBJ_DIR)\replay.obj: src\replay.cpp src\include\replay.h
	$(CC) $(CFLAGS) /c src\replay.cpp /Fo$(OBJ_DIR)\replay.obj

$(OBJ_DIR)\replay_progress.obj: src\replay_progress.cpp src\include\replay_progress.h
	$(CC) $(CFLAGS) /c src\replay_progress.cpp /Fo$(OBJ_DIR)\replay_progress.obj

$(OBJ_DIR)\scene.obj: src\scene.cpp src\include\scene.h
	$(CC) $(CFLAGS) /c src\scene.cpp /Fo$(OBJ_DIR)\scene.obj

$(OBJ_DIR)\shader.obj: src\shader.cpp src\include\shader.h
	$(CC) $(CFLAGS) /c src\shader.cpp /Fo$(OBJ_DIR)\shader.obj

$(OBJ_DIR)\sound.obj: src\sound.cpp
	$(CC) $(CFLAGS) /c src\sound.cpp /Fo$(OBJ_DIR)\sound.obj

$(OBJ_DIR)\splash.obj: src\splash.cpp src\include\splash.h
	$(CC) $(CFLAGS) /c src\splash.cpp /Fo$(OBJ_DIR)\splash.obj
$(OBJ_DIR)\subregion_box.obj: src\subregion_box.cpp src\include\subregion_box.h
	$(CC) $(CFLAGS) /c src\subregion_box.cpp /Fo$(OBJ_DIR)\subregion_box.obj

$(OBJ_DIR)\subregion_modal.obj: src\subregion_modal.cpp src\include\subregion_modal.h src\include\subregion_box.h
	$(CC) $(CFLAGS) /c src\subregion_modal.cpp /Fo$(OBJ_DIR)\subregion_modal.obj

$(OBJ_DIR)\stats.obj: src\stats.cpp src\include\stats.h
	$(CC) $(CFLAGS) /c src\stats.cpp /Fo$(OBJ_DIR)\stats.obj

$(OBJ_DIR)\render_pipeline.obj: src\render_pipeline.cpp src\include\render_pipeline.h
	$(CC) $(CFLAGS) /c src\render_pipeline.cpp /Fo$(OBJ_DIR)\render_pipeline.obj

$(OBJ_DIR)\stb_impl.obj: src\stb_impl.cpp
	$(CC) $(CFLAGS) /c src\stb_impl.cpp /Fo$(OBJ_DIR)\stb_impl.obj

$(OBJ_DIR)\text_renderer.obj: src\text_renderer.cpp src\include\text_renderer.h
	$(CC) $(CFLAGS) /c src\text_renderer.cpp /Fo$(OBJ_DIR)\text_renderer.obj

$(OBJ_DIR)\Renderer2D.obj: src\Renderer2D.cpp src\include\Renderer2D.h
	$(CC) $(CFLAGS) /c src\Renderer2D.cpp /Fo$(OBJ_DIR)\Renderer2D.obj

$(OBJ_DIR)\tickbox.obj: src\tickbox.cpp src\include\tickbox.h
	$(CC) $(CFLAGS) /c src\tickbox.cpp /Fo$(OBJ_DIR)\tickbox.obj

$(OBJ_DIR)\tomography.obj: src\tomography.cpp src\include\tomography.h
	$(CC) $(CFLAGS) /c src\tomography.cpp /Fo$(OBJ_DIR)\tomography.obj

# --- Model ---
$(OBJ_DIR)\initSim.obj: src\model\initSim.cpp src\include\model\simulation.h
	$(CC) $(CFLAGS) /c src\model\initSim.cpp /Fo$(OBJ_DIR)\initSim.obj

$(OBJ_DIR)\interaction.obj: src\model\interaction.cpp src\include\model\simulation.h
	$(CC) $(CFLAGS) /c src\model\interaction.cpp /Fo$(OBJ_DIR)\interaction.obj

$(OBJ_DIR)\simulation.obj: src\model\simulation.cpp src\include\model\simulation.h
	$(CC) $(CFLAGS) /c src\model\simulation.cpp /Fo$(OBJ_DIR)\simulation.obj

$(OBJ_DIR)\utils.obj: src\model\utils.cpp src\include\model\simulation.h
	$(CC) $(CFLAGS) /c src\model\utils.cpp /Fo$(OBJ_DIR)\utils.obj

$(OBJ_DIR)\bridge.obj: src\model\bridge.cpp src\include\model\simulation.h src\include\voxel.h
	$(CC) $(CFLAGS) /c src\model\bridge.cpp /Fo$(OBJ_DIR)\bridge.obj

$(OBJ_DIR)\geometry.obj: src\model\geometry.cpp src\include\model\geometry.h
	$(CC) $(CFLAGS) /c src\model\geometry.cpp /Fo$(OBJ_DIR)\geometry.obj

$(OBJ_DIR)\polarization.obj: src\model\polarization.cpp src\include\model\simulation.h src\include\model\polarization.h
	$(CC) $(CFLAGS) /c src\model\polarization.cpp /Fo$(OBJ_DIR)\polarization.obj

$(OBJ_DIR)\charges.obj: src\model\charges.cpp src\include\model\simulation.h
	$(CC) $(CFLAGS) /c src\model\charges.cpp /Fo$(OBJ_DIR)\charges.obj

# --- Libraries ---
$(OBJ_DIR)\glad.obj: glad\glad.c
	$(CC) $(CFLAGS) /c glad\glad.c /Fo$(OBJ_DIR)\glad.obj
$(OBJ_DIR)\tinyfiledialogs.obj: src\tinyfiledialogs.c src\include\tinyfiledialogs.h
	$(CC) $(CFLAGS) /c src\tinyfiledialogs.c /Fo$(OBJ_DIR)\tinyfiledialogs.obj

# --- AC Project (new spherical core) ---
# Removed 2026-09-20: these rules pointed at tests\ac_project\, a directory that
# does not exist in this tree, so nmake could not build them anyway.  The model
# sources in src\model\ are the only core this build links.
# ================================================
# DLLs
# ================================================

dlls:
	copy "$(VCPKG_ROOT)\bin\glfw3.dll" $(BUILD_DIR)
	copy "$(VCPKG_ROOT)\bin\freetype.dll" $(BUILD_DIR)
	copy "$(VCPKG_ROOT)\bin\zlib1.dll" $(BUILD_DIR)
	copy "$(VCPKG_ROOT)\bin\bz2.dll" $(BUILD_DIR)
	copy "$(VCPKG_ROOT)\bin\brotlidec.dll" $(BUILD_DIR)
	copy "$(VCPKG_ROOT)\bin\brotlicommon.dll" $(BUILD_DIR)
	copy "$(VCPKG_ROOT)\bin\brotlienc.dll" $(BUILD_DIR)
	copy "$(VCPKG_ROOT)\bin\libpng16.dll" $(BUILD_DIR)

# Runtime assets (fonts, logos): copied next to the executable so the
# program runs from the build directory (nmake run) without relying on
# relative-path fallbacks.  The font search in TextRenderer/Logo also
# tolerates missing assets by trying several candidate locations.
assets:
	if exist "bin\fonts" xcopy /E /I /Y "bin\fonts" "$(BUILD_DIR)\fonts"
	if exist "logo.png"      copy /Y "logo.png"      "$(BUILD_DIR)"
	if exist "logo_bar.png"  copy /Y "logo_bar.png"  "$(BUILD_DIR)"
	if exist "bin\logo.png"      copy /Y "bin\logo.png"      "$(BUILD_DIR)"
	if exist "bin\logo_bar.png"  copy /Y "bin\logo_bar.png"  "$(BUILD_DIR)"

# ================================================
# Dependencia global de headers
#
# As regras por ficheiro listam apenas o seu proprio header.  Um header
# partilhado alterado (config.h, globals.h, model\simulation.h, ...) nao
# recompilava os dependentes, deixando objectos compilados com um layout de
# struct antigo -- um ABI silenciosamente errado (foi o que produziu o arranque
# com o campo de visao em 0.1 graus, i.e. "zoom no maximo", em 15/09/2026).
# Esta regra SEM comandos obriga a recompilar todos os objectos quando
# qualquer header muda.  nmake nao tem $(wildcard): a lista abaixo e explicita,
# por isso um header NOVO tem de ser acrescentado aqui (ou os objectos ficam
# com o layout antigo).
# ================================================

ALL_HEADERS = \
	src\include\animator.h \
	src\include\app_context.h \
	src\include\button.h \
	src\include\callbacks.h \
	src\include\camera.h \
	src\include\color_utils.h \
	src\include\config.h \
	src\include\core.h \
	src\include\cortina.h \
	src\include\draw_utils.h \
	src\include\glm_config.h \
	src\include\globals.h \
	src\include\GUI.h \
	src\include\GUI3D.h \
	src\include\help.h \
	src\include\hslider.h \
	src\include\hud.h \
	src\include\input.h \
	src\include\layers.h \
	src\include\logo.h \
	src\include\menubar.h \
	src\include\mouse_helper.h \
	src\include\progress.h \
	src\include\projection.h \
	src\include\projection_manager.h \
	src\include\radio.h \
	src\include\recorder.h \
	src\include\renderer.h \
	src\include\Renderer2D.h \
	src\include\render_pipeline.h \
	src\include\replay.h \
	src\include\replay_progress.h \
	src\include\scene.h \
	src\include\shader.h \
	src\include\sinc_overlay.h \
	src\include\splash.h \
	src\include\subregion_box.h \
	src\include\subregion_modal.h \
	src\include\stats.h \
	src\include\stb_image.h \
	src\include\text_renderer.h \
	src\include\thread_safety.h \
	src\include\tickbox.h \
	src\include\tinyfiledialogs.h \
	src\include\tomography.h \
	src\include\voxel.h \
	src\include\vslider.h \
	src\include\model\attractor.h \
	src\include\model\chief_transition.h \
	src\include\model\color_contact.h \
	src\include\model\election_payload.h \
	src\include\model\geometry.h \
	src\include\model\island_identity.h \
	src\include\model\polarization.h \
	src\include\model\polarization_candidate.h \
	src\include\model\simulation.h \
	src\include\model\wavefront.h \
	src\include\zlib\cuda\cuda_common.h \
	src\include\zlib\cuda\cuda_sim_optimized.h

$(OBJ): $(ALL_HEADERS)

# ================================================
# Limpeza
# ================================================

clean:
	@echo Limpando objetos...
	-@del /Q /F $(OBJ_DIR)\*.obj 2>nul

	@echo Limpando binarios...
	-@del /Q /F $(BUILD_DIR)\*.exe 2>nul
	-@del /Q /F $(BUILD_DIR)\*.dll 2>nul
	-@del /Q /F $(BUILD_DIR)\*.pdb 2>nul

# ================================================
# Configuração
# ================================================

copy_config:
	if exist automaton.cfg copy automaton.cfg "$(BUILD_DIR)\automaton.cfg"

# ================================================
# Execução
# ================================================

run:
	cd "$(BUILD_DIR)" && $(TARGET)

# ================================================
# Documents-vs-artifacts lint (the claims lint, revived over what exists in this tree)
# ================================================
# The 2026-09-19 lint (its note CLAIMS_LINT.md and its script check_claims.py) lived over the
# whole paper-facing tooling and was retired to the study archive, which is not part of this
# repository (E:\alpha\attic\closed_programmes\).  This target is its in-tree
# successor and checks only claims that can be checked here: the cited paths, the FSM.txt
# registry against the #ifdefs of the sources, and the machine-checkable invariants against the
# trace logs in build\ (a log that is not built is a note, never a failure).  Warnings do not
# fail; errors do.  `check-claims` stays as an alias, so the old name does not fail with
# "no rule to make target".

check-docs:
	powershell -NoProfile -ExecutionPolicy Bypass -File experiments\check_docs.ps1

check-claims: check-docs
	@echo check-claims: renamed to check-docs on 2026-09-24 -- the lint above ran instead

# ================================================
# Trace regression (golden logs)
# ================================================
# experiments\check_trace.bat builds four configurations of the harness -- the reference and the
# three of the promotion decision (ref, disp, base, bothab) -- runs each on one scenario (L=7,
# sieve closed, 6 frames = one era, ending on the era-1 cascade frame) and compares the log with
# the expectation committed in experiments\golden\.  That scenario is what separates the four:
# nothing, the dispersal alone, the encounter's transport (94 steps at f6) and the same with the
# pair rules (46 steps).  The model is deterministic (no RNG, no address, no scan order), so a
# difference is a change in the dynamics or in the scenario and never noise; the runs are the
# cost, about 70 s each.
#   nmake check-trace             compare; non-zero exit on any difference
#   nmake refresh-trace-golden    accept what was produced as the new expectation (then commit)

check-trace:
	cmd /c experiments\check_trace.bat

refresh-trace-golden:
	cmd /c experiments\check_trace.bat refresh

# ================================================
# ODR/link gate (header regression check)
# ================================================
# Two tiny TUs include the shared headers and are linked together.  A header
# that DEFINES a function or variable with external linkage without "inline"
# fails here with LNK2005/LNK1169 instead of breaking the full GUI build.
# History: 2026-09-17, model/simulation.h defined isqrt non-inline
# (commit 3cc70c4) and the GUI could no longer link; the last successful
# executable predated that commit.
check-odr: dirs
	$(CC) $(CFLAGS) /c experiments\odr_gate_tu1.cpp experiments\odr_gate_tu2.cpp /Fo"$(OBJ_DIR)\\"
	$(CC) $(OBJ_DIR)\odr_gate_tu1.obj $(OBJ_DIR)\odr_gate_tu2.obj /Fe:$(OBJ_DIR)\odr_gate.exe /link /SUBSYSTEM:CONSOLE
	@echo [check-odr] OK - shared headers link cleanly (no duplicate symbols)

# ================================================
# Unlinked diagnostics (compile-only check)
# ================================================
# src\model\attractor.cpp and src\model\wavefront.cpp are read-only diagnostics:
# nothing in the simulation calls them, so they are deliberately kept out of
# OBJ_COMMON and out of automaton.exe -- but they must keep compiling, and their
# headers are already part of the ODR gate.  This target compiles them into obj\
# and links nothing.
check-extras: dirs
	$(CC) $(CFLAGS) /c src\model\attractor.cpp src\model\wavefront.cpp /Fo"$(OBJ_DIR)\\"
	@echo [check-extras] OK - unlinked diagnostics compile

# ================================================
# First-era trace (headless; no GUI, no CUDA)
# ================================================
# experiments\first_era_trace.cpp drives the model sources directly and prints one
# line per light frame of the expansion era: the shell radius and cell count, the
# attractor census (K, D, S, P halves, unresolved), the number of distinct charge
# words over the W source addresses, how many of those addresses are structurally
# pairable under R1..R6, and the active-shell size.  It links no GUI and no CUDA:
# the four GUI-side globals (voxels, convol_delay, diffuse_delay, reloc_delay) are
# stubbed inside the harness.  Objects go to obj\trace\ so they never shadow the
# GUI build's own objects.  Run from the repository root, where automaton.cfg lives:
#   nmake trace-first-era
#   build\first_era_trace.exe 9 16384 8      (L, s2b_target, frames)
# The trace target adds /D "S2B_TRACE" on top of $(CFLAGS): the harness's annotated counters (the
# census and clock lines, move-align/split/drift/mask/carrier) only exist under that macro -- without
# it the model compiles the plain path and those lines print zeros, which is how this target's output
# was read as "the drift and mask buckets are empty" once.  The .bat scripts of experiments\ define it
# as well.  The dynamics do not depend on it (it is counters and two per-layer vectors).
TRACE_SRC   = experiments\first_era_trace.cpp
TRACE_MODEL = src\config.cpp src\model\initSim.cpp src\model\simulation.cpp \
	src\model\interaction.cpp src\model\utils.cpp src\model\geometry.cpp \
	src\model\polarization.cpp src\model\charges.cpp src\model\attractor.cpp \
	src\model\wavefront.cpp

trace-first-era: dirs
	@echo [trace-first-era] config: $(OBJ_DIR) $(CANDIDATE_FLAGS)
	if not exist "$(OBJ_DIR)\trace" mkdir "$(OBJ_DIR)\trace"
	$(CC) $(CFLAGS) /D "S2B_TRACE" $(TRACE_SRC) $(TRACE_MODEL) /Fo"$(OBJ_DIR)\trace\\" /Fe:$(BUILD_DIR)\first_era_trace.exe /link /SUBSYSTEM:CONSOLE
	@echo [trace-first-era] built $(BUILD_DIR)\first_era_trace.exe

rebuild:
	nmake clean
	nmake

# ================================================
# Targets simbólicos
# ================================================

.SYMBOLIC: clean run rebuild all dirs dlls copy_config check-odr check-extras check-docs check-trace refresh-trace-golden trace-first-era
