# Continuidade — MH Store ERP

Revisão: 20/09/2026. A prioridade atual está em [docs/proximas-entregas.md](docs/proximas-entregas.md). A primeira entrega, backup automático e recuperação, foi implementada; a próxima é **robustez operacional e diagnóstico**.

## Como retomar

Leia o roteiro de próximas entregas, [ESTRUTURA_DO_PROJETO.md](ESTRUTURA_DO_PROJETO.md) e [docs/progresso.md](docs/progresso.md). Confira o código e o Git antes de alterar: registros antigos de progresso são históricos. Marque caixas somente após implementação, testes e publicação. Não considerar o ERP pronto para comercialização.

O produto continua modular e offline, com uma instalação SQLite por cliente. O usuário autoriza desenvolvimento por etapas e publicação no repositório `hendelsantos/ERP_Cplus_plus_Gestao`, branch `main`.

## Estado atual

- C++20, Qt 6/QML, SQLite; **esquema 20**. Esta entrega não adiciona migração.
- Autenticação local, administrador/operador, permissões, recuperação e auditoria; consultar `docs/autenticacao.md`.
- Cadastros, estoque, caixa, PDV, vendas, financeiro e serviços já possuem implementações. O catálogo inclui variações e o PDV suporta quantidades fracionadas; consultar o código/testes para regras específicas.
- Módulos e dependências são controlados no backend. Dinheiro usa centavos inteiros; preservar transações de venda, pagamento e estoque.
- Backup manual e automático, configuração administrativa, execução em segundo plano, retenção limitada às cópias automáticas da instalação e validação de integridade.
- Agendamento desativado por padrão; ao ativar, verifica ao iniciar e a cada minuto, executa quando vencido e após fechamento do caixa. Configuração em `<banco>.backup.ini`, externa ao backup.
- Restauração valida backups versões 1–20, migra uma cópia temporária e exige estrutura final idêntica ao banco atual. Não modifica o arquivo original. Exige caixa fechado/carrinho vazio, gera cópia anterior e substitui os dados em transação; sessão e aplicativo são encerrados após sucesso.
- Testes específicos cobrem restauração antiga (4, 8 e 9), versão atual, rollback, corrupção, retenção, falha de destino e configuração na interface.

## Validação e comandos

```bash
./scripts/dev.sh build
./scripts/dev.sh test
./scripts/dev.sh run
```

Executável: `build/bin/MHStore`. O script prepara caminhos do Qt em `~/.local/share/mhstore-qt`. OpenSSL Crypto é dependência de compilação. Testes usam bancos temporários e autenticação real, sem acessar os dados do cliente.

Suíte completa: cadastros, estoque, caixa/PDV, financeiro, backup e interface (6 conjuntos). Windows, máquina limpa de cliente e GPU real precisam de validação própria.

## Próximo passo

Seguir a entrega 2 de `docs/proximas-entregas.md`: logging local com rotação e proteção de dados sensíveis, diagnóstico de versões/caminhos, integridade na inicialização e testes de falhas recuperáveis. Preservar funcionamento offline e atualizar checklist e evidências a cada entrega validada.

Instaladores, demonstrações por segmento, recursos comerciais finais e licenciamento vêm depois, na ordem do roteiro. Emissão fiscal e sincronização em nuvem têm escopo próprio.
