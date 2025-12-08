
# Table of Contents

1.  [Nome do Desafio Tecnológico](#orgb6dfae4)
    1.  [GeoLogViewer-FerramentaDeLeituraEVisualizacaoDeDadosGeof-sicosDeArquivos-LAS](#org96230b0)
    2.  [https://github.com/ldsc/GeoLogViewer-FerramentaDeLeituraEVisualizacaoDeDadosGeof-sicosDeArquivos-LAS/blob/main/1-PreProjeto-IdentificacaoDaProposta/lyx/1-PreProjeto-Master.pdf](#org284962d)
2.  [Nome do Software:](#org984145a)
3.  [Resumo e/ou informação extra:](#org917980e)
4.  [Versão:](#org93c2766)
5.  [Data:](#orgdf4b675)
6.  [Autores de Contato:](#orga547495)
7.  [Lista dos Autores:](#org98691e0)
8.  [Áreas de pesquisa vinculadas:](#orge2f1b72)
9.  [Vínculo com:](#orga0431a5)
10. [Paradigmas:](#org5e3f5a7)
11. [Tipo de Interface:](#org1e9ae5e)
12. [Plataformas Suportadas:](#orged101f2)
13. [Linguagens Utilizadas:](#org111a827)
14. [Bibliotecas/Softwares Utilizados (Dependências):](#org13cf1d6)
15. [Funcionalidades Principais:](#org9425aba)
16. [Manual do Usuário e Desenvolvedor:](#orge110abe)
17. [Instalação e Compilação:](#orge05c7a4)
18. [Trabalhos Futuros e Sugestões de Melhoria:](#org44fd5c4)
19. [Licença:](#org678c7b5)



<a id="orgb6dfae4"></a>

# Nome do Desafio Tecnológico


<a id="org96230b0"></a>

## GeoLogViewer-FerramentaDeLeituraEVisualizacaoDeDadosGeof-sicosDeArquivos-LAS


<a id="org284962d"></a>

## <https://github.com/ldsc/GeoLogViewer-FerramentaDeLeituraEVisualizacaoDeDadosGeof-sicosDeArquivos-LAS/blob/main/1-PreProjeto-IdentificacaoDaProposta/lyx/1-PreProjeto-Master.pdf>


<a id="org984145a"></a>

# Nome do Software:

-   GeoLogViewer


<a id="org917980e"></a>

# Resumo e/ou informação extra:

-   O GeoLogViewer é uma ferramenta computacional desenvolvida em C++ destinada à leitura, visualização e interpretação básica de perfis geofísicos de poço no formato LAS (Log ASCII Standard).
-   O software permite a plotagem de curvas (Gamma Ray, Resistividade, Porosidade, etc.) utilizando o Gnuplot como motor gráfico e realiza cálculos petrofísicos básicos, como volume de argila (Vshale) e identificação de zonas de reservatório.


<a id="org93c2766"></a>

# Versão:

-   1.0 (Reflete a entrega final v0.3 com interpretação).


<a id="orgdf4b675"></a>

# Data:

-   2025/2 - Segundo semestre.


<a id="orga547495"></a>

# Autores de Contato:

-   Pedro Daneluci Cardozo <pedrocardozo@lenep.uenf.br>
-   Rafael Moreira Silva <rafaelsilva@lenep.uenf.br>


<a id="org98691e0"></a>

# Lista dos Autores:

-   Pedro Daneluci Cardozo
-   Rafael Moreira Silva
-   Prof. André Duarte Bueno (Orientador) <bueno@lenep.uenf.br>


<a id="orge2f1b72"></a>

# Áreas de pesquisa vinculadas:

-   Engenharia de Petróleo e Exploração
-   Petrofísica
-   Modelagem Matemática Computacional
-   Engenharia de Software


<a id="orga0431a5"></a>

# Vínculo com:

-   Trabalho da disciplina LEP01348: Introdução ao Projeto de Engenharia.
-   Laboratório de Engenharia e Exploração de Petróleo (LENEP/UENF).


<a id="org5e3f5a7"></a>

# Paradigmas:

-   Orientação a Objetos (C++).


<a id="org1e9ae5e"></a>

# Tipo de Interface:

-   Texto (CLI/Terminal) com saída gráfica via janela externa (Gnuplot).


<a id="orged101f2"></a>

# Plataformas Suportadas:

-   Windows
-   Linux


<a id="org111a827"></a>

# Linguagens Utilizadas:

-   C++ (Standard 11 ou superior).


<a id="org13cf1d6"></a>

# Bibliotecas/Softwares Utilizados (Dependências):

-   ****Gnuplot:**** Necessário estar instalado e acessível via PATH do sistema para a geração dos gráficos.
    Veja <https://github.com/ldsc/LDSC-Ajuda-DocumentosAuxiliares/tree/main/02-Softwares/03-Softwares/02-Gnuplot>
-   Biblioteca Padrão do C++ (STL).


<a id="org9425aba"></a>

# Funcionalidades Principais:

1.  ****Leitura de Arquivos LAS:**** Extração de metadados, cabeçalhos e dados das curvas.
2.  ****Visualização (Plotting):**** Geração de gráficos de perfis simples e múltiplos (Tracks).
3.  ****Interpretação Petrofísica:****
    -   Cálculo de Vshale (Volume de Argila).
    -   Estimativa de Porosidade.
    -   Identificação automática de zonas de interesse (reservatório).
4.  ****Exportação:**** Geração de relatórios de interpretação em formato CSV.


<a id="orge110abe"></a>

# Manual do Usuário e Desenvolvedor:

-   Os manuais completos em PDF encontram-se na pasta `docs/` deste repositório ou na seção de Releases.


<a id="orge05c7a4"></a>

# Instalação e Compilação:

    # Exemplo genérico de compilação via g++
    g++ main.cpp src/*.cpp -o GeoLogViewer

-   É necessário ter o **Gnuplot** instalado no sistema operacional.


<a id="org44fd5c4"></a>

# Trabalhos Futuros e Sugestões de Melhoria:

Para futuras versões ou continuidade do projeto por outros desenvolvedores, sugerem-se as seguintes implementações:

1.  ****Interface Gráfica (GUI):****
    -   Substituir a interface de terminal por uma interface gráfica robusta utilizando a biblioteca ****Qt****.
    -   Criar janelas de diálogo para seleção de arquivos e configuração de parâmetros.
2.  ****Bibliotecas de Plotagem Integradas:****
    -   Migrar do motor externo Gnuplot para bibliotecas de plotagem nativas do C++/Qt, como ****QCustomPlot****, permitindo interação em tempo real (zoom, pan, seleção de intervalos) diretamente na janela da aplicação.
3.  ****Expansão da Petrofísica:****
    -   Adicionar novos modelos de cálculo de saturação de água (Archie, Simandoux).
    -   Implementar cross-plots (ex: Neutrão-Densidade).
4.  ****Otimização:****
    -   Melhorar o parser de arquivos LAS para suportar versões mais recentes (LAS 3.0) e lidar com arquivos corrompidos de forma resiliente.


<a id="org678c7b5"></a>

# Licença:

-   GPL / Acadêmica (Software Livre para fins educacionais).

