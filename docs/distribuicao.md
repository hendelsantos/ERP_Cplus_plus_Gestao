# Distribuição Linux — versão de validação

Alvo inicial: Ubuntu 24.04, arquitetura do ambiente de compilação. Esta entrega não é um instalador Windows e não declara o sistema pronto para comercialização.

## Gerar o pacote

Com Docker disponível, execute na raiz do projeto:

```bash
./scripts/package-linux.sh
```

O processo compila em Ubuntu 24.04, executa a suíte, exporta o `.deb` para `dist/` e valida o pacote em outro contêiner limpo. Não instala pacotes no computador de desenvolvimento. A imagem e os repositórios Ubuntu recebem atualizações: o procedimento é repetível, mas não promete artefatos idênticos byte a byte entre datas diferentes.

## Instalar e abrir

```bash
sudo apt install ./mhstore_0.1.0_amd64.deb
```

O apt instala Qt, plugins, módulos QML, driver SQLite, OpenSSL e outras bibliotecas necessárias. A instalação inicial exige acesso aos repositórios Ubuntu (ou dependências previamente disponíveis). O pacote não inclui uma coleção de dependências para instalação sem internet; o uso do sistema depois de instalado continua offline.

Abra **MH Store** no menu de aplicativos ou execute `MHStore`. Não execute o aplicativo com sudo. `mhstore-diagnostico` imprime um resumo técnico em JSON, sem exportar cadastros, credenciais ou conteúdo de logs. Os caminhos podem identificar o usuário local; confira o relatório antes de compartilhar.

## Dados, atualização e remoção

O executável fica em `/usr/bin/MHStore`; arquivos de menu, ícone e documentação ficam em `/usr/share`. Dados ficam em `$XDG_DATA_HOME/MHSoftware/MH Store` ou, por padrão, `~/.local/share/MHSoftware/MH Store`. Banco, backups, logs e configurações locais não fazem parte do pacote.

Antes de atualizar, faça um backup verificado, feche o aplicativo e instale o novo `.deb` usando apt. A próxima abertura verifica a integridade e aplica migrações transacionais. Bancos com versão posterior à suportada são rejeitados; não faça downgrade do executável sobre um banco já atualizado.

```bash
sudo apt remove mhstore
```

Remoção e purge não apagam os dados do usuário: o pacote não contém scripts que removem diretórios pessoais. Reinstalar com o mesmo usuário e diretório de dados permite retomar a operação. Mudar usuário ou `XDG_DATA_HOME` seleciona outro diretório de dados.

Se uma atualização não iniciar, preserve o banco e a cópia anterior. Não substitua arquivos SQLite enquanto houver instância aberta. Use o diagnóstico e o procedimento de recuperação com suporte; não há restauração destrutiva automática.

## Repetir a validação do pacote

```bash
./scripts/verify-package-linux.sh dist/mhstore_0.1.0_amd64.deb
```

A validação instala o pacote e suas dependências em uma imagem Ubuntu sem Qt de desenvolvimento. Executa o aplicativo como usuário comum com renderização offscreen, verifica carga de QML/plugins, reinstalação preservando dados, migração de uma base de teste 19 para 20, purge/reinstalação e rejeição de esquema futuro. Também verifica o relatório de suporte. As intervenções no banco usam exclusivamente dados sintéticos dentro do contêiner.

Esse teste não substitui abrir o atalho em um desktop real, nem simula atualização entre dois executáveis de versões distintas. O pacote permanece de validação até concluir os testes gráficos e operacionais em máquina de cliente.

## Referências técnicas

- [Implantação Qt no Linux](https://doc.qt.io/qt-6/linux-deployment.html): bibliotecas e plugins precisam estar disponíveis no destino.
- [Dependências de bibliotecas Debian](https://www.debian.org/doc/debian-policy/ch-sharedlibs.html): o pacote usa `dpkg-shlibdeps` via CPack, além das dependências QML carregadas em tempo de execução.

Validação em ambiente isolado é descrita no progresso do projeto. Menu gráfico real, impressão, GPU, instalação em máquina física e Windows exigem verificações adicionais.
