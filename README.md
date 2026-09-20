# MH Store ERP

Repositório: **ERP_Cplus_plus_Gestao**.

ERP desktop offline-first da MHSoftware, construído com C++20, Qt 6, QML e SQLite.

## Estado atual

Cadastros básicos de **produtos, categorias, clientes e fornecedores**, com busca, edição e inativação/reativação; produtos podem ser vinculados a um fornecedor. **Estoque** com entradas, saídas, ajustes por contagem, filtro de saldo crítico e histórico. Os dados são persistidos localmente. **Caixa e PDV básicos** com abertura, suprimento/sangria, carrinho, pagamento, baixa de estoque e fechamento. Dashboard com indicadores reais de vendas, estoque e caixa; os demais módulos seguem pendentes.

Siga a [estrutura e roteiro do projeto](ESTRUTURA_DO_PROJETO.md) para orientar as próximas entregas.

Veja a [comparação com o plano e próximas etapas](docs/progresso.md). Para retomar o desenvolvimento, leia [CONTINUAR_AQUI.md](CONTINUAR_AQUI.md).

O plano operacional das próximas entregas está em [docs/proximas-entregas.md](docs/proximas-entregas.md).

## Compilar, testar e abrir nesta máquina

O Qt 6.4.2 foi preparado em `~/.local/share/mhstore-qt`, a partir de pacotes Ubuntu, sem instalação administrativa. O script configura as bibliotecas e os plugins apenas para o comando executado:

```bash
./scripts/dev.sh build
./scripts/dev.sh test
./scripts/dev.sh run
```

O executável fica em `build/bin/MHStore`. O diretório `build/MHStore` contém arquivos do módulo QML. As configurações do CMake no VS Code também apontam para o Qt local.

## Outros ambientes

Pré-requisitos: CMake 3.21+, compilador C++20 e Qt 6.4+ com Quick, Quick Controls, SQL, driver SQLite e Test, além de OpenSSL Crypto (desenvolvimento). A instalação do Qt deve ser encontrada por `Qt6_DIR` ou `CMAKE_PREFIX_PATH`.

```bash
cmake -S desktop -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/bin/MHStore
```

No Ubuntu 24.04, os pacotes de desenvolvimento e execução incluem:

```bash
sudo apt install libssl-dev qt6-base-dev qt6-declarative-dev libqt6sql6-sqlite \
  qml6-module-qtqml qml6-module-qtqml-models qml6-module-qtqml-workerscript \
  qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-qtquick-templates qml6-module-qtquick-window
```

Para compilar sem os testes e sem depender de Qt Test, configure com `-DBUILD_TESTING=OFF`. A configuração local de `.vscode/settings.json` deve ser adaptada se usar outra instalação do Qt.

O banco `mhstore.sqlite` é criado no diretório de dados da aplicação, resolvido por `QStandardPaths::AppDataLocation`. Os testes usam bancos temporários separados; o teste da interface utiliza a plataforma Qt offscreen.

## Usar os cadastros

1. Abra **Categorias** e cadastre uma categoria.
2. Em **Produtos**, clique em **Novo cadastro**, informe nome, código interno e valores, e salve. O **Fornecedor** é opcional; fornecedores inativos deixam de ser oferecidos, mas vínculos existentes são preservados.
3. Em **Clientes**, cadastre nome e os contatos desejados.
4. Em **Fornecedores**, cadastre nome e, se houver, documento (único), telefone e e-mail. Vários fornecedores podem ficar sem documento.
5. Use a busca e **Editar** para consultar e alterar registros. **Mostrar inativos** permite localizar e reativar cadastros.

Valores aceitam vírgula ou ponto decimal, sem separador de milhar.

## Movimentar estoque

1. Cadastre um produto e abra **Estoque**.
2. Clique em **Movimentar** e escolha **Entrada**, **Saída** ou **Ajuste por contagem**.
3. Informe quantidade e motivo; o responsável é preenchido pela sessão. No ajuste, informe o **saldo final contado**; zero é permitido.
4. Clique em **Registrar**. O saldo e o histórico são gravados juntos.

Quantidades aceitam até três casas decimais. Saídas não podem tornar o saldo negativo. Produtos inativos mantêm seu histórico, mas precisam ser reativados para receber movimentações. O filtro **Estoque crítico** mostra produtos ativos com saldo menor ou igual ao mínimo. O histórico apresenta as últimas 200 movimentações, de todos os produtos ou do produto selecionado, com horário local.

O responsável vem da sessão autenticada e seu identificador é gravado na movimentação. Movimentação manual exige administrador. Inventário em lote e custo médio seguem pendentes; cadastros possuem auditoria própria.


## Realizar uma venda

1. Cadastre um produto com preço de venda maior que zero e registre uma entrada de estoque.
2. Abra **Caixa**, informe o dinheiro inicial, e clique em **Abrir caixa**.
3. Abra **PDV**, pesquise e clique nos produtos para adicioná-los ao carrinho. Use `+`, `−` ou **Remover** para alterar os itens.
4. Escolha a forma de pagamento. Para dinheiro, informe o valor recebido; o sistema calcula o troco.
5. Clique em **Finalizar venda**. A venda, os itens, o pagamento, o saldo e o histórico de estoque são gravados juntos. Um resumo aparece na tela.
6. Para encerrar, volte a **Caixa**, informe o dinheiro contado e clique em **Fechar caixa**. O histórico mostra esperado, contado e diferença.

A busca aceita nome, código interno ou código de barras. Enter adiciona o produto quando existe apenas uma correspondência. Esta versão vende unidades inteiras, até 10.000 por item, com uma forma de pagamento por venda. PIX e cartões são registros manuais, sem integração ou confirmação bancária. Apenas pagamentos em dinheiro entram no saldo físico esperado do caixa.

Preços do PDV, totais, pagamentos e conferência de caixa usam centavos inteiros. A migração converte os valores existentes; os campos REAL antigos são mantidos como espelho para compatibilidade com os cadastros. Mudanças de preço, inativação ou falta de estoque após adicionar ao carrinho impedem a finalização e solicitam revisão.

Ainda faltam ajustes percentuais, venda fracionada, pagamentos divididos, cancelamento/devolução e impressão/exportação do comprovante. O carrinho e o resumo da última venda ficam em memória; as vendas finalizadas permanecem no SQLite. O resumo na tela não é documento fiscal.


## Suprimento e sangria

Com uma sessão aberta, vá a **Caixa** e escolha **Suprimento** para adicionar dinheiro à gaveta ou **Sangria** para retirar. Informe valor e motivo e clique em **Registrar movimentação**.

O saldo esperado passa a ser: **abertura + vendas em dinheiro + suprimentos − sangrias**. Valores são calculados em centavos inteiros. Sangrias acima do saldo disponível e movimentações de sessões fechadas são rejeitadas.

Cada sessão mostra os totais de suprimentos e sangrias. Clique em **Movimentações** para consultar os últimos 200 registros manuais daquela sessão, inclusive após o fechamento, com data/hora local, motivo, responsável e saldos anterior/final. Esse histórico é específico de entradas/retiradas manuais; vendas continuam registradas separadamente.

O responsável é identificado pela sessão autenticada. Não há edição/exclusão de movimentações pela interface; uma correção em caixa aberto deve ser registrada como nova movimentação, com motivo claro.


## Consultar vendas anteriores

Abra **Vendas** para listar os registros mais recentes. A listagem possui páginas de até 50 vendas; use **Anterior** e **Próxima** para navegar ou informe o número exato da venda para pesquisar.

Clique em **Ver venda** para consultar data/hora local, caixa, responsável, itens, preços, total, forma de pagamento, valor recebido e troco. Os itens mostram o código, nome e preço gravados no momento da venda, mesmo após alterar ou inativar o produto.

As consultas funcionam após reiniciar o aplicativo. Registros antigos que não possuem itens ou pagamento detalhado são identificados sem inventar essas informações. O histórico aceita filtro opcional por período no formato `AAAA-MM-DD` e permite exportar os resultados para CSV UTF-8 ou PDF.


## Dashboard

O painel apresenta faturamento do dia e do mês, vendas do dia, ticket médio diário, recebimentos em dinheiro e por outros meios, cancelamentos do mês, produtos com estoque crítico e dinheiro esperado na sessão de caixa aberta. Também mostra a quantidade de produtos ativos sem estoque. Os indicadores são calculados a partir das vendas, pagamentos e movimentações persistidos; contas a pagar e receber ainda não fazem parte do sistema.

## Financeiro

A seção **Financeiro** permite lançar despesas a pagar e contas a receber com vencimento. A baixa de despesas cria uma sangria e o recebimento cria um suprimento, ambos na sessão de caixa aberta e dentro da mesma transação que atualiza o título.

No PDV, produtos com unidade como KG podem ser vendidos com quantidades de até três casas decimais. O total da linha é arredondado para centavos e o estoque mantém a quantidade fracionada.

Após finalizar uma venda, o comprovante não fiscal pode ser exportado para PDF pelo diálogo de venda concluída.
Se houver uma impressora padrão configurada, o mesmo diálogo oferece impressão física; sem impressora, o sistema mantém a exportação PDF disponível.

Produtos podem registrar tamanho, cor e grupo de variação opcionais. Ao selecionar um produto agrupado no PDV, o sistema apresenta as variantes disponíveis e seus saldos antes de adicionar a escolhida ao carrinho.

O cadastro também permite marcar um item como **Serviço**. Serviços podem ser vendidos no PDV sem alterar estoque ou criar movimentos de inventário. A seção Financeiro permite abrir ordens de serviço vinculadas a cliente e serviço, acompanhar descrição, observações e status; consumo de materiais ainda está pendente.
Materiais podem ser associados à OS por identificador e quantidade; ao concluir a ordem, o estoque é validado e baixado em uma única transação.

Vendas consideradas: somente as concluídas, agrupadas pelo horário local do computador. Estoque crítico inclui produtos ativos com saldo menor ou igual ao mínimo; produtos sem estoque também entram nessa contagem. Sem vendas, o ticket médio é zero. Sem caixa aberto, o saldo é zero e o painel informa que não há sessão aberta.

O painel atualiza ao abrir o aplicativo, ao voltar para **Dashboard**, a cada minuto enquanto está visível ou pelo botão **Atualizar painel**. Uma falha de consulta exibe indicadores indisponíveis, em vez de apresentar dados antigos como atuais. Indicadores financeiros, gráficos e exportações ainda estão pendentes.


## Backup local e restauração

Em **Configurações → Backup local**, informe o caminho completo da pasta e clique em **Criar backup**. O arquivo `.mhb` contém uma cópia SQLite consistente, validada antes de informar sucesso. Pode ser criado com o caixa aberto. **Listar arquivos** mostra os backups presentes na pasta, com data de modificação, tamanho, integridade e versão do esquema; não é um registro de auditoria de operações.

Para restaurar:

1. Feche o caixa atual e finalize ou limpe o carrinho.
2. Selecione um backup da lista ou informe o caminho completo do arquivo.
3. Clique em **Restaurar backup** e confira a confirmação.
4. O sistema verifica integridade, vínculos e compatibilidade, cria uma cópia `antes_restauracao_*.mhb` na subpasta `backups` do diretório de dados e restaura em transação.
5. Após o sucesso, o aplicativo fecha. Abra novamente com `./scripts/dev.sh run`.

O esquema atual é **20**. Backups das versões 1 a 20 passam por validação e migração em uma cópia temporária; a estrutura resultante deve corresponder ao banco atual. O arquivo original é preservado. Versões futuras e estruturas incompatíveis são rejeitadas. Se o backup foi feito com um caixa aberto, essa sessão também será recuperada. Falhas de restauração desfazem as alterações de dados. A cópia anterior permite recuperar o estado que existia antes da restauração.

Os backups incluem somente o banco SQLite, sem criptografia, anexos ou configurações externas. Guarde também cópias em outra unidade; a cópia na mesma unidade não protege contra perda do disco. Compactação e backup em nuvem seguem pendentes. Evite editar o banco com ferramentas externas durante o uso; o aplicativo permite apenas uma instância por diretório de dados.

A cópia consistente utiliza [VACUUM INTO, documentado pelo SQLite](https://www.sqlite.org/lang_vacuum.html#vacuum_with_an_into_clause). A restauração verifica as relações com [foreign_key_check](https://www.sqlite.org/pragma.html#pragma_foreign_key_check).


### Agendamento local

Em **Agendar backups**, o administrador pode ativar cópias automáticas, escolher uma pasta absoluta, intervalo de 1 a 10.080 minutos e retenção de 1 a 100 arquivos. O recurso vem desativado. Verifica o vencimento ao iniciar e a cada minuto enquanto o aplicativo está aberto; fechar o caixa também solicita uma cópia. Funciona sem login, após configuração autorizada, usando conexão SQLite própria em segundo plano. Ao sair, aguarda uma cópia já iniciada terminar.

Somente uma cópia íntegra recebe extensão `.mhb`. O nome automático identifica instalação, esquema e horário UTC; a lista apresenta a data local do arquivo. A retenção só remove cópias automáticas desta instalação depois de concluir uma nova cópia válida: backups manuais, cópias anteriores à restauração e arquivos de outras instalações são preservados. Falhas aparecem na tela de backup. Falhas de criação não avançam o horário de sucesso, permitindo nova tentativa.

A política fica em `<caminho-do-banco>.backup.ini`, fora do SQLite e do Git, e não é substituída ao restaurar dados. Trocar o caminho do banco muda a identificação da instalação usada na retenção. A restauração encerra o agendamento e a sessão; reabra o aplicativo após concluir.

## Cliente na venda

Cadastre o cliente em **Clientes** e selecione-o no campo **Cliente** do PDV antes de finalizar. A identificação é opcional: **Consumidor não identificado** mantém a venda sem vínculo. Somente clientes ativos aparecem na seleção; o cliente é validado novamente na finalização.

Após uma venda concluída ou ao limpar o carrinho, a seleção volta para consumidor não identificado. A seleção é preservada ao navegar entre telas; se o cliente for inativado, escolha outro ou remova a identificação antes de vender.

Em **Vendas → Ver venda**, aparecem o código do cliente e o nome conforme o cadastro atual. O vínculo permanece se o cliente for inativado. O nome histórico do cliente não é armazenado separadamente; alterações de nome no cadastro aparecem nas consultas anteriores. Veja a compatibilidade atual de backups na seção de backup. O histórico de compras pode ser aberto pelo cadastro do cliente.


## Histórico de compras por cliente

Em **Clientes**, clique em **Compras** no cadastro desejado. Para consultar um cliente inativo, marque **Mostrar inativos**.

A tela mostra as vendas concluídas desse cliente em páginas de até 50 registros, quantidade de compras, total gasto e última compra em horário local. Os indicadores consideram todo o histórico concluído, mesmo ao mudar de página ou pesquisar um número de venda. Vendas anônimas e canceladas não entram nessa consulta. Clientes sem compras apresentam quantidade e total zero.

Use **Ver venda** para abrir os detalhes, **Voltar para clientes** para retornar ao cadastro ou **Todas as vendas** para remover o filtro e a busca por número. O nome mostrado acompanha o cadastro atual. A consulta permanece disponível após reiniciar o aplicativo. Veja a compatibilidade atual de backups na seção de backup.


## Empresa e módulos

Abra **Configurações → Empresa e módulos** para salvar o nome da empresa, perfil do negócio e habilitar Estoque, Caixa e PDV. O nome aparece no título da janela. Os perfis disponíveis são comércio em geral, roupas e acessórios, mercado e mercearia e serviços. Nesta etapa, o perfil identifica o negócio; ele não implementa grade, venda por peso ou ordens de serviço.

O PDV atual exige Estoque e Caixa. Alterações nos módulos exigem caixa fechado e carrinho vazio. Desabilitar preserva os dados e bloqueia operações de escrita do módulo no C++, além de removê-lo do menu. Cadastros, histórico de vendas, dashboard e backup continuam disponíveis. Configuração de módulos é separada das permissões dos usuários. Autenticação local já está disponível; licenciamento permanece pendente.

A migração **5** preserva os dados existentes e inicia todos os módulos habilitados, com nome “Minha empresa”. As configurações ficam no SQLite e são incluídas no backup. Backups antigos são migrados em cópia temporária antes da restauração, conforme a seção de backup.


O menu e as opções de módulos usam um registro central com identificador, nome e dependências. A configuração rejeita módulos desconhecidos, seleções incompletas e valores inválidos. As verificações operacionais consultam também as dependências do módulo. Veja a versão atual do esquema e a compatibilidade na seção de backup.


## Usuários e login offline

No primeiro acesso, crie o administrador com nome, login (3–40 letras/números/ponto/traço/underscore) e senha de 12–128 caracteres. O sistema mostra um código de recuperação: guarde-o fora do computador, confirme que o guardou e faça login. Nenhuma chamada de rede é necessária para criar ou validar usuários.

Em **Usuários**, o administrador cria e edita contas, escolhe o perfil, redefine senhas e inativa/reativa acessos. Login é único e não diferencia maiúsculas. Na edição, senha vazia mantém a atual. Não há exclusão de usuários. O próprio cadastro (nome, login, perfil) é alterado por outro administrador nesta versão.

**Minha senha**, no menu lateral, permite a qualquer usuário autenticado trocar a própria senha informando a senha atual. A sessão e o carrinho são mantidos; sessões anteriores são invalidadas e o código de recuperação existente continua válido.

| Operação | Administrador | Operador de caixa |
| --- | --- | --- |
| Consultar cadastros, estoque, vendas e dashboard | Sim | Sim |
| Operar PDV e caixa habilitados | Sim | Sim |
| Alterar cadastros e movimentar estoque manualmente | Sim | Não |
| Configurar empresa/módulos, backup e restauração | Sim | Não |
| Gerenciar usuários e consultar a auditoria | Sim | Não |

As permissões são verificadas no C++. Após cinco tentativas inválidas, a conta fica bloqueada por cinco minutos; esse estado persiste no SQLite. Para sair, finalize ou limpe o carrinho. O caixa pode permanecer aberto para troca de operador. Vendas, movimentos de estoque, movimentos de caixa e abertura/fechamento gravam o ID do usuário e seu nome da ocasião. Registros anteriores mantêm seus nomes históricos e não recebem um usuário inventado.

**Esqueci minha senha** usa o login de um administrador ativo, seu código e uma nova senha. O código é de uso único: a recuperação emite outro código e invalida o anterior. Administradores adicionais recebem seu código de outro administrador, no botão **Código de recuperação** de **Usuários**, exibido uma só vez ao emissor. Alterar uma conta pela administração invalida suas sessões e seu código de recuperação; operadores têm suas senhas redefinidas por um administrador. Não existe senha mestra nem recuperação por e-mail nesta versão.

**Auditoria**, no menu lateral do administrador, lista as alterações administrativas no momento em que ocorreram: usuários criados/alterados, senhas trocadas, códigos de recuperação emitidos, configurações de empresa/módulos e alterações de cadastros. Cada registro mostra o responsável, o alvo, os detalhes e o horário local; senhas e códigos nunca são gravados. A listagem é paginada e a consulta é restrita a administradores.

A migração **6** adiciona usuários e vínculos às operações existentes, sem criar senha padrão; a migração **7** adiciona a auditoria administrativa; a migração **8** adiciona fornecedores e o vínculo com produtos; a **9**, campos complementares. Backups incluem usuários, credenciais protegidas, auditoria e fornecedores; após restaurar, valem os registros presentes no backup. O aplicativo encerra a sessão e deve ser reaberto. Backups antigos seguem a migração controlada descrita na seção de backup.

Senhas usam PBKDF2-HMAC-SHA256 com salt aleatório individual e 600.000 iterações via OpenSSL. O código de recuperação tem 256 bits aleatórios e somente seu hash é persistido. Detalhes e limites: [autenticação local](docs/autenticacao.md).


## Campos complementares

- **Empresa e módulos:** documento (até 20 caracteres), telefone (20) e endereço (160).
- **Clientes:** endereço (160), nascimento opcional em DD/MM/AAAA e observações (500). A data deve existir e não pode ser futura; é armazenada como AAAA-MM-DD.
- **Produtos:** marca (60), unidade (10), localização (60), observações (500) e estoque máximo. Zero significa máximo não definido; quando positivo, deve ser maior ou igual ao mínimo.

Todos esses campos são opcionais. O estoque máximo é informativo, sem bloquear entradas ou gerar alertas; a unidade também é informativa e não habilita venda fracionada. Documento e telefone são textos informativos, sem validação fiscal ou integração externa.

A migração **9** preserva os registros existentes e inicia os campos novos vazios, com máximo zero. O backup inclui esses dados. A restauração valida e migra uma cópia temporária de backups antigos para o esquema atual.


## Auditoria dos cadastros

Em **Auditoria**, administradores consultam criações, alterações, inativações e reativações de produtos, categorias, clientes e fornecedores. Cada evento informa usuário, horário local, tipo e ID do cadastro. Criações e edições listam os nomes dos campos envolvidos; mudanças de status indicam ativo/inativo. Os valores dos campos não são copiados para esse histórico.

Alteração e auditoria são gravadas juntas: se o registro de auditoria falhar, o cadastro permanece como estava. Salvar valores idênticos ou repetir o status atual não gera evento. Tentativas inválidas ou sem permissão também não geram eventos de alteração concluída. O histórico começa com as novas operações; não há reconstrução retroativa de mudanças anteriores.

Esta entrega reutiliza `audit_log` e não exigiu migração própria. Veja a versão atual do esquema na seção de backup. Não há edição/exclusão de eventos pela interface. O histórico não substitui backup nem oferece proteção contra edição direta do SQLite fora do aplicativo.


## Descontos e acréscimos no PDV

Com produtos no carrinho, um administrador pode abrir **Desconto / acréscimo**, informar valores em reais e uma justificativa de até 200 caracteres e clicar em **Aplicar ajuste**. O desconto deve ser menor que o subtotal; valores negativos, mais de duas casas decimais e total acima do limite da venda são rejeitados. Ambos os ajustes podem ser usados na mesma venda. Para remover, aplique zero nos dois campos.

O PDV mostra subtotal, ajustes e total final. O pagamento, o troco, o caixa e os indicadores usam o total final; os itens preservam preços e totais anteriores ao ajuste. Adicionar, remover ou mudar a quantidade de itens limpa os ajustes para exigir nova revisão. Limpar o carrinho ou concluir a venda também os remove. Falha de finalização mantém o carrinho e os ajustes.

A permissão `pos.adjust` está disponível somente ao administrador na matriz atual e é revalidada na finalização. O ajuste concluído gera auditoria junto da venda, pagamento e baixa de estoque; qualquer falha desfaz a transação. Resumo e detalhes da venda exibem os ajustes e a justificativa. PIX/cartões continuam registros manuais.

A migração **10** acrescenta subtotal, desconto, acréscimo e justificativa. Vendas antigas recebem subtotal igual ao total existente e ajustes zero, sem inventar histórico. Backups atuais incluem esses campos; a restauração segue a compatibilidade descrita na seção de backup. Ajustes percentuais, ajustes por item e rateio para devoluções não estão implementados.

## Diagnóstico e recuperação na inicialização

Administradores podem consultar **Configurações → Empresa e módulos → Diagnóstico** para ver versões, caminhos e configurar o nível de log. Logs locais possuem rotação e registram eventos fixos sem SQL, credenciais ou dados dos clientes. A inicialização detecta sessões interrompidas e verifica a integridade do SQLite antes de migrar; falhas apresentam orientação e preservam o banco. Consulte [docs/diagnostico.md](docs/diagnostico.md) para funcionamento e limites.

## Pacote Linux de validação

A geração do instalador Ubuntu 24.04 está em `./scripts/package-linux.sh`. O processo usa Docker para compilar, testar, gerar o `.deb` em `dist/` e verificar a instalação isoladamente. O apt resolve as dependências; não é necessário preparar Qt manualmente no computador do cliente. A instalação inicial pode exigir internet, mas a operação permanece offline.

Consulte [docs/distribuicao.md](docs/distribuicao.md) para instalação, atualização, remoção sem apagar dados e diagnóstico. A distribuição Windows e os testes em desktop físico continuam pendentes.
