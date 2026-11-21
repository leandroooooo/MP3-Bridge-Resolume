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

A pasta `vst2-probe/` contém um plugin mínimo de teste VST2 para Windows x64. Ele foi criado para verificar se o Resolume Arena carrega um plugin VST2 externo. O plugin apenas deixa o áudio passar com um controle simples de ganho; ele ainda não abre nem reproduz MP3.

O workflow `.github/workflows/build-vst2-probe.yml` compila a DLL e publica o artefato:

`MP3BridgeVST2Probe.dll`

Após baixar o artefato, coloque a DLL em uma pasta VST2 que o Resolume já esteja examinando, como a pasta Extra Effects do usuário, e reinicie o Arena. Confirme se o nome **MP3 Bridge VST2 Probe** aparece em Audio Effects. Não substitua nem remova os plugins existentes.

## Teste VST3 anterior

O workflow `.github/workflows/build-vst3-probe.yml` compila os exemplos AGain da Steinberg em VST3. Esse teste é separado e não comprova suporte do Arena a VST3.

## Limites e próximos passos

O teste VST2 é apenas uma prova de reconhecimento/carregamento. Mesmo que apareça em Audio Effects, isso não significa que um efeito VST possa funcionar como uma fonte/clipe de áudio arrastável. Depois de confirmar o carregamento, precisamos validar qual ponto de extensão do Resolume permite chegar ao fluxo desejado: arrastar o MP3 para uma camada e apertar Play.

O diagnóstico MP3 confirmou que o Windows conseguiu decodificar o arquivo de teste para PCM. Isso é uma etapa necessária, mas não resolve por si só a importação direta no Arena.

Referências:
- https://learn.microsoft.com/windows/win32/medfound/supported-media-formats-in-media-foundation
- https://learn.microsoft.com/windows/win32/medfound/source-reader
- https://learn.microsoft.com/pt-br/windows/win32/medfound/tutorial--decoding-audio
