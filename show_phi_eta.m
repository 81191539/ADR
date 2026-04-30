clc;
clear;
close all;
remarks_1;

case_number = 2;
ss = sprintf('remarks_%d.m', case_number);
run(ss)

x=linspace(0,xright,nx+1);
y=linspace(0,yright,ny+1);
[xx,yy]= meshgrid(x,y);
[y2,x2]= ndgrid(x,y);

%% 输出动画
Include_movie = 1;  % 0 不输出动图，1 输出动图

if Include_movie == 1
    cp = sprintf('para_evolve_Pe_%0.2f_Pe2_%0.2f_lam_%0.4f_alpha0.2_%d', Pe, Pe2,lambda, alpha);  %avi格式输出
    aviobj=VideoWriter(cp);%新建叫example.avi的文件
    aviobj.FrameRate = 3;
    open(aviobj); %打开example.avi的文件
end

if Include_movie == 1
    mpgobj=VideoWriter(cp, 'MPEG-4'); % 使用 H.264 编码的 MPEG-4 文件（Windows 7 或更高版本或者 Mac OS X 10.7 及更高版本的系统）
    mpgobj.Quality = 95;
    mpgobj.FrameRate = 3;  % 调整显示帧数,可以控制动画的快慢
    open(mpgobj); %打开example.avi的文件
end

BN = 0;

NN = 10;

step = 1;

num = 1;  % 用于控制time_eta_F.m文件中包含时间的每一行数据

for it= BN:step:NN
    
    figure(it+1)
        
    fig1 = figure('PaperSize',[20.98 29.68]);
    axes('Parent',fig1,'Position',[0.3552 0.5838 0.3347 0.3412]);

    %%   第一幅图
    subplot(3,2,[1 2 ],'Parent',fig1);

    ss=sprintf('data_%d/cc_%d.m', case_number, it );
    ide=load(ss);

    rx1 = [-0.1 xpo_l ];  
    rx2 = [xpo_l xpo_r ];
    rx3 = [xpo_r xright+0.1];
    ry1 = [-0.2 -0.01]; 
    ry2 = [1+0.01 1.2];

   hold on 
   lw = 1.25;
  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%
   plot([rx1(1), rx1(2)],[ry1(1), ry1(1) ], 'k-', 'lineWidth', lw)
   plot([rx1(1), rx1(2)],[ry1(2), ry1(2) ], 'k-', 'lineWidth', lw)
   plot([rx1(1), rx1(1)],[ry1(1), ry1(2) ], 'k-', 'lineWidth', lw)
   plot([rx1(2), rx1(2)],[ry1(1), ry1(2) ], 'k-', 'lineWidth', lw)
   rct1 = [rx1(1) ry1(1)   
           rx1(1) ry1(2)
           rx1(2) ry1(2)
           rx1(2) ry1(1)];
   patch(rct1(:,1),rct1(:,2), [0.7 0.7 0.7])

   plot([rx2(1), rx2(2)],[ry1(1), ry1(1) ], 'k-', 'lineWidth', lw)
   plot([rx2(1), rx2(2)],[ry1(2), ry1(2) ], 'k-', 'lineWidth', lw)
   plot([rx2(1), rx2(1)],[ry1(1), ry1(2) ], 'k-', 'lineWidth', lw)
   plot([rx2(2), rx2(2)],[ry1(1), ry1(2) ], 'k-', 'lineWidth', lw)
   rct2 = [rx2(1) ry1(1)   
           rx2(1) ry1(2)
           rx2(2) ry1(2)
           rx2(2) ry1(1)];
   patch(rct2(:,1),rct2(:,2), [0.0 0.4 0])

   plot([rx3(1), rx3(2)],[ry1(1), ry1(1) ], 'k-', 'lineWidth', lw)
   plot([rx3(1), rx3(2)],[ry1(2), ry1(2) ], 'k-', 'lineWidth', lw)
   plot([rx3(1), rx3(1)],[ry1(1), ry1(2) ], 'k-', 'lineWidth', lw)
   plot([rx3(2), rx3(2)],[ry1(1), ry1(2) ], 'k-', 'lineWidth', lw)
   rct3 = [rx3(1) ry1(1)   
           rx3(1) ry1(2)
           rx3(2) ry1(2)
           rx3(2) ry1(1)];
   patch(rct3(:,1),rct3(:,2), [0.7 0.7 0.7])
   
   %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
      
   plot([rx1(1), rx3(2)],[ry2(1), ry2(1) ], 'k-', 'lineWidth', lw)
   plot([rx1(1), rx3(2)],[ry2(2), ry2(2) ], 'k-', 'lineWidth', lw)
   %plot([rx3(1), rx3(1)],[ry2(1), ry2(2) ], 'k-', 'lineWidth', lw)
   %plot([rx3(2), rx3(2)],[ry2(1), ry2(2) ], 'k-', 'lineWidth', lw)
   rct4 = [rx1(1) ry2(1)   
           rx1(1) ry2(2)
           rx3(2) ry2(2)
           rx3(2) ry2(1)];
   patch(rct4(:,1),rct4(:,2), [0.7 0.7 0.7])

 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
   
    mesh(y2, x2, ide)
   
    %sp=sprintf('$Pe$ = %d , $\lambda$ = %4.3f', Pe, lambda);
    %txt = title(sp);
    %set(txt, 'Interpreter', 'latex');
    
%     format short 
%     lambda = roundn(lambda,-4);
%     Pe = roundn(Pe,-2);
%     Pe2 = roundn(Pe2,-2);
%     alpha = roundn(alpha,-2);
    
    cp = sprintf('Pe1 = %0.2f, Pe2 = %0.2f, lam = %0.4f, alpha = %0.2f', Pe, Pe2, lambda, alpha); 
    %txt = title('$Pe = 30$ , $\lambda = 0.0625$ ');
    txt = title(cp);
    set(txt, 'Interpreter', 'latex', 'FontSize', 25);

    %%%%%%% 调整时间显示
    sss = num2str(it);
    time_x = strcat("time_",sss); 
    ex = eval(time_x);
    ex = roundn(ex,-3);
    text('Interpreter', 'latex', 'String', ex, ...
        'Position', [1.4 1.85], 'FontSize', 10)  
    text('Interpreter', 'latex', 'String', '$t^* = $', ...
        'Position', [-0.8 1.85], 'FontSize', 11)
    text('Interpreter', 'latex', 'String', '$c^*$', ...
        'Position', [-0.85 -0.9], 'FontSize', 12)

    
    set(gca,'fontsize',8)
    yticks(0: 0.5: 1)
    xticks(0: 1: 16)
    

    % axis square
    % view(20, 70)
    
    view(0, 90)    
    colormap jet
    
    ch = colorbar('horiz');% 横向坐标轴

%     set(get(ch,'title'),'string','[m]','position',[590 15]);% title的位置，590代表左右，-15代表上下，可以不加position发现默认位置在colorar中间
%     set(get(ch,'title'),'string','[m]','position',[590 -15]);% title的位置，590代表左右，-15代表上下，可以不加position发现默认位置在colorar中间
%     set(ch,'position',[0.135 0.075 0.75 0.015],'ticks',(-1:0.2:1),'ticklength',0.015,'fontsize',12, ...
%            'ticklabels',{'<-1.0',(-0.8:0.2:0.8),'>1.0'}) % colorbar的位置，[左 下 宽 高]
      set(ch,'position',[0.13 0.72 0.775 0.025],'fontsize', 10)
           
      
    caxis([0 1])
    
        %box on
    axis off
    axis image
    
    %axis([0-1, xright+1, 0-0.5, yright+0.5])

    
    
    %hold on
   %%  第二幅图
   %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    subplot(3,2,[3 4],'Parent',fig1);
    
    ss=sprintf('data_%d/ee_%d.m',case_number, it);
    ee=load(ss);
    plot(ee(:,1), ee(:,2), 'r-', 'LineWidth', 1.5)

    xticks(xleft: 2: xright)
    yticks(yleft: 0.2: 0.6)    
    set(gca, 'fontsize',10)    
    xlabel('\it x', 'Interpreter', 'latex', 'FontSize', 13, 'position',[6.93 -0.095])
    ylabel('$\eta$', 'Interpreter', 'latex', 'FontSize', 13)
        
    
    axis([xleft, xright, 0, 0.6 ])
    %title('\eta','fontsize',13)
    daspect([5 1 1])
    %axis square
    grid on
%     axis image 
    
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% 
    ee=load('time_eta_F_data.m');
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  第三幅图
    subplot(3,2,5,'Parent',fig1);  
    plot(ee(1:num,1), ee(1:num,2), 'r-', 'LineWidth', 1.5)
    
    set(gca, 'fontsize',10)  % 注意这里用gac会对当前图所有字体大小起作用, 
                             % 想单独起作用的, 在单独的命令中再加字体控制参数!
    yticks(0: 0.2: 0.6)  % 调整刻度值显示
    xticks(0: 2: endT)    % 调整刻度值显示
    
    ylabel('$\bar\eta$','Interpreter', 'latex', 'FontSize', 13)  % 调整label显示方式
    xlabel('$t^*$','Interpreter', 'latex', 'FontSize', 13)  % 调整label显示方式
    
    grid on
    axis([0, endT, -0.05, 0.6])
    daspect([15 1 1])   % 调整长宽比,固定第二三个参数为1,调整第一个参数大小

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  第四幅图
    subplot(3,2,6,'Parent',fig1);
    plot(ee(1:num,1), ee(1:num,3), 'r-', 'LineWidth', 1.5)
   
    set(gca, 'fontsize',10)
    
    yticks(0: 0.2: 0.6)  % 调整刻度值显示
    xticks(0: 2: endT)    % 调整刻度值显示
    
    ylabel('$\mathcal{F}$','Interpreter', 'latex', 'FontSize', 11)
    xlabel('$t^*$','Interpreter', 'latex', 'FontSize', 12)
   
    grid on
    axis([0, endT, -0.05, 0.6])
    daspect([15 1 1])   % 调整长宽比,固定第二三个参数为1,调整第一个参数大小
    
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% 
    num = num+1;   
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% 
    
    if Include_movie == 1
        rect=[0 0 543 429];
        currFrame = getframe(gcf,rect);
        writeVideo(aviobj,currFrame);        
    end
    
    if Include_movie == 1
        rect=[0 0 543 420];
        currFrame = getframe(gcf,rect);
        writeVideo(mpgobj,currFrame);        
    end
    
    
end

if Include_movie == 1
    close(mpgobj);
end

if Include_movie == 1
    close(aviobj);
end
%%%plot average eta in another figure
% figure(1000)
% 
% ss=sprintf('time_eta_F_data.m');
% ee=load(ss);
% plot(ee(:,1), ee(:,2), 'r-', 'LineWidth', 1)
%         
% ylabel('$\eta$','Interpreter', 'latex', 'FontSize', 16)
% xlabel('$t^*$','Interpreter', 'latex', 'FontSize', 16)
% set(gca, 'fontsize',15)
% 
% axis square
% grid off
% axis([0, 40, 0, 0.6])

% for it = BN:step:NN
%     
%     figure(it+100)
%     
%     ss=sprintf('data/ee_%d.m', it);
%     ee=load(ss);
%     plot(ee(:,1), ee(:,2), 'bo-')
%     
%     title('\eta')
%     
%     %axis square
%     axis image 
% end




