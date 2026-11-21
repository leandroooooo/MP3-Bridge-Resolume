# MP3 Bridge for Resolume — diagnóstico e testes de integração

O objetivo do projeto é investigar uma forma de usar arquivos MP3 diretamente no Resolume Arena, sem conversão manual.

## Diagnóstico MP3 no Windows

A etapa `diagnostic/` testa se o Windows consegue abrir um MP3 e decodificá-lo para PCM usando Microsoft Media Foundation. Ela não instala nada no Resolume.

Para compilar localmente:

```bat
cmake -S diagnostic -B build -A x64
cmake --build build --config Release
```

O executável será criado em `build\Release\Mp3DecodeCheck.exe`.

## Teste de reconhecimento VST2

A pasta `vst2-probe/` contém um plugin mínimo de teste VST2 para Windows x64. Ele verifica se o Resolume Arena carrega um plugin VST2 externo e recebe/processa o áudio de um clip.

A versão atual é um **pass-through estéreo transparente**: não altera o som e não possui controle de ganho próprio. O controle Dry/Wet exibido pelo Resolume é do próprio host. O plugin ainda não abre nem reproduz MP3.

O workflow `.github/workflows/build-vst2-probe.yml` compila a DLL e publica o artefato:

`MP3BridgeVST2Probe.dll`

Após baixar o artefato, coloque a DLL em uma pasta VST2 que o Resolume já esteja examinando, como a pasta Extra Effects do usuário, e reinicie o Arena. Confirme se o nome **MP3 Bridge VST2 Probe** aparece em Audio Effects. Não substitua nem remova os plugins existentes.

## Teste VST3 anterior

O workflow `.github/workflows/build-vst3-probe.yml` compila os exemplos AGain da Steinberg em VST3. Esse teste é separado e não comprova suporte do Arena a VST3.

## Limites técnicos e próximo passo

O teste VST2 confirma apenas o caminho de **efeito de áudio**: o Resolume entrega áudio PCM do clip ao plugin, e o plugin devolve áudio processado. Um efeito VST2 não recebe automaticamente o caminho do arquivo MP3 que foi arrastado para a grade e não se transforma em uma fonte/clipe de áudio.

O decoder Media Foundation já foi validado separadamente no Windows. Para cumprir o fluxo desejado — arrastar um MP3 diretamente para a grade e apertar Play — ainda precisamos identificar uma interface de fonte/clipe de áudio oficialmente suportada pelo Resolume. Não devemos apresentar um VST de efeito como se já resolvesse essa parte.

O diagnóstico MP3 confirmou que o Windows conseguiu decodificar o arquivo de teste para PCM. Isso é uma etapa necessária, mas não resolve por si só a importação direta no Arena.

Referências:
- https://learn.microsoft.com/windows/win32/medfound/supported-media-formats-in-media-foundation
- https://learn.microsoft.com/windows/win32/medfound/source-reader
- https://learn.microsoft.com/pt-br/windows/win32/medfound/tutorial--decoding-audio
