# Caminho para a primeira versão comercial

Direção: software desktop offline-first modular, com base comum e extensões por segmento. A versão atual é de desenvolvimento. Comercialização no Mercado Livre é um objetivo; publicação e condições da plataforma ainda precisam ser verificadas antes do lançamento.

## Base já disponível

Cadastros, estoque, caixa/PDV básicos, consulta de vendas, histórico por cliente, dashboard, backup/restauração local e configuração inicial de empresa/módulos. Validação atual em Linux com Qt 6.4; Windows ainda não validado.

## Etapas necessárias

1. Base modular: registro central de módulos/dependências já disponível; faltam usuários, login, permissões e auditoria. Configuração atual de módulos não constitui licença ou controle de acesso.
2. Operação comercial: fornecedores, financeiro, descontos, pagamentos divididos, cancelamento/devolução transacional e comprovantes.
3. Segmentos: definir requisitos e testar separadamente grade de roupas, unidades/peso e ordens de serviço. Selecionar um perfil hoje não fornece essas funcionalidades.
4. Confiabilidade: evolução de backups antigos, backup automático, recuperação testada, testes de falhas e atualização preservando dados.
5. Distribuição: instalador Windows, dependências Qt, atualização, licenciamento, diagnóstico e validação em máquina limpa.
6. Entrega comercial: manual, demonstração, escopo e limitações publicados, processo de suporte e recuperação; verificar condições atuais de anúncio/distribuição na plataforma escolhida.

## Critérios de liberação

- Fluxos prometidos ao segmento demonstrados de ponta a ponta em Windows.
- Instalação, atualização, backup e restauração validados sem perda de dados.
- Permissões e operações sensíveis testadas, sem depender apenas de ocultar telas.
- Recursos pendentes não anunciados como disponíveis. Comprovante não fiscal deve permanecer identificado como tal.
- Versão, documentação e canal de suporte definidos para cada entrega.

Não há data de lançamento definida. Cada etapa deve entregar código funcional, testes e documentação antes de ser considerada concluída.
