# MP3 Bridge for Resolume — diagnóstico inicial

Esta etapa testa apenas se o Windows consegue abrir um MP3 e decodificá-lo para PCM usando Microsoft Media Foundation. Não instala nada no Resolume e não injeta código no Arena.

## Compilar no Windows

É necessário ter Visual Studio 2022 Build Tools (com C++) e CMake.

Abra um terminal na pasta `diagnostic` e execute:

```bat
cmake -S . -B build -A x64
cmake --build build --config Release
```

O executável será criado em `build\Release\Mp3DecodeCheck.exe`.

## Executar o teste

```bat
Mp3DecodeCheck.exe "C:\caminho\para\uma-musica.mp3"
```

Se aparecer `SUCESSO`, o Windows abriu o arquivo e leu dados de áudio PCM até o final.

## GitHub Actions

O workflow `.github/workflows/build-diagnostic.yml` compila automaticamente no Windows e publica o executável como artefato na execução do workflow.

## Limite importante

Este teste não faz o Resolume aceitar MP3. Ele confirma somente que existe uma rota de decodificação MP3 no Windows. A integração para arrastar um MP3 diretamente para uma camada do Arena é uma etapa separada e depende de um ponto de extensão suportado pelo próprio Resolume.

Referências:
- https://learn.microsoft.com/windows/win32/medfound/supported-media-formats-in-media-foundation
- https://learn.microsoft.com/windows/win32/medfound/source-reader
- https://learn.microsoft.com/pt-br/windows/win32/medfound/tutorial--decoding-audio
