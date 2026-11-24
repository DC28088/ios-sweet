
/* 26c: the blit encoder (see tools/prepare-sweet-native-metal.py adapt_blit). Every copy is recorded as a
 * SweetNativeMetalWork and runs in order with the draws and dispatches at -commit. */
@interface SweetNativeMetalBlitEncoder : NSObject {
@public
	id _cb;
	NSString *_label;
}
@end
@implementation SweetNativeMetalBlitEncoder
+ (void)load
{
	const char *names[] = { "MTLCommandEncoder", "MTLBlitCommandEncoder", "MTLBlitCommandEncoderSPI" };
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
		Protocol *p = objc_getProtocol(names[i]);
		if (p != NULL && !class_conformsToProtocol(self, p)) class_addProtocol(self, p);
	}
}
- (void)dealloc { [_cb release]; [_label release]; [super dealloc]; }
- (id)device { return [_cb device]; }
- (NSString *)label { return _label; }
- (void)setLabel:(NSString *)label { [_label release]; _label = [label copy]; }
- (void)endEncoding {}
- (void)insertDebugSignpost:(NSString *)s { (void)s; }
- (void)pushDebugGroup:(NSString *)s { (void)s; }
- (void)popDebugGroup {}
- (void)updateFence:(id)fence { (void)fence; }
- (void)waitForFence:(id)fence { (void)fence; }
- (void)synchronizeResource:(id)r { (void)r; }
- (void)synchronizeTexture:(id)t slice:(NSUInteger)s level:(NSUInteger)l { (void)t; (void)s; (void)l; }
- (void)optimizeContentsForGPUAccess:(id)t { (void)t; }
- (void)optimizeContentsForGPUAccess:(id)t slice:(NSUInteger)s level:(NSUInteger)l { (void)t; (void)s; (void)l; }
- (void)optimizeContentsForCPUAccess:(id)t { (void)t; }
- (void)optimizeContentsForCPUAccess:(id)t slice:(NSUInteger)s level:(NSUInteger)l { (void)t; (void)s; (void)l; }
- (void)nativeRecord:(SweetNativeMetalWork *)work { [_cb recordWork:work]; [work release]; }
- (void)copyFromTexture:(id)src sourceSlice:(NSUInteger)ss sourceLevel:(NSUInteger)sl sourceOrigin:(MTLOrigin)so
             sourceSize:(MTLSize)size toTexture:(id)dst destinationSlice:(NSUInteger)ds destinationLevel:(NSUInteger)dl
      destinationOrigin:(MTLOrigin)dorg
{
	if (src == nil || dst == nil) return;
	SweetNativeMetalWork *work = [SweetNativeMetalWork new];
	work->nativeBlit = 1; work->color[0] = src; work->color[1] = dst;
	work->blitSrcSlice = (unsigned)ss; work->blitSrcLevel = (unsigned)sl; work->blitDstSlice = (unsigned)ds; work->blitDstLevel = (unsigned)dl;
	work->blitSrcOrigin[0] = (unsigned)so.x; work->blitSrcOrigin[1] = (unsigned)so.y; work->blitSrcOrigin[2] = (unsigned)so.z;
	work->blitSize[0] = (unsigned)size.width; work->blitSize[1] = (unsigned)size.height; work->blitSize[2] = (unsigned)size.depth;
	work->blitDstOrigin[0] = (unsigned)dorg.x; work->blitDstOrigin[1] = (unsigned)dorg.y; work->blitDstOrigin[2] = (unsigned)dorg.z;
	[self nativeRecord:work];
}
- (void)copyFromTexture:(id)src sourceSlice:(NSUInteger)ss sourceLevel:(NSUInteger)sl sourceOrigin:(MTLOrigin)so
             sourceSize:(MTLSize)size toTexture:(id)dst destinationSlice:(NSUInteger)ds destinationLevel:(NSUInteger)dl
      destinationOrigin:(MTLOrigin)dorg options:(NSUInteger)options
{
	(void)options;
	[self copyFromTexture:src sourceSlice:ss sourceLevel:sl sourceOrigin:so sourceSize:size toTexture:dst
	     destinationSlice:ds destinationLevel:dl destinationOrigin:dorg];
}
- (void)copyFromTexture:(id)src sourceSlice:(NSUInteger)ss sourceLevel:(NSUInteger)sl toTexture:(id)dst
       destinationSlice:(NSUInteger)ds destinationLevel:(NSUInteger)dl sliceCount:(NSUInteger)slices levelCount:(NSUInteger)levels
{
	if (src == nil || dst == nil) return;
	for (NSUInteger l = 0; l < levels; l++)
		for (NSUInteger i = 0; i < slices; i++) {
			NSUInteger w = [src width] >> (sl + l), h = [src height] >> (sl + l);
			NSUInteger d = [src textureType] == MTLTextureType3D ? [src depth] >> (sl + l) : 1;
			MTLSize size = MTLSizeMake(w ? w : 1, h ? h : 1, d ? d : 1);
			[self copyFromTexture:src sourceSlice:ss + i sourceLevel:sl + l sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:size
			            toTexture:dst destinationSlice:ds + i destinationLevel:dl + l destinationOrigin:MTLOriginMake(0, 0, 0)];
		}
}
- (void)copyFromTexture:(id)src toTexture:(id)dst
{
	if (src == nil || dst == nil) return;
	NSUInteger levels = MIN([src mipmapLevelCount], [dst mipmapLevelCount]), slices = MIN([src arrayLength], [dst arrayLength]);
	if ([src textureType] == MTLTextureTypeCube || [src textureType] == MTLTextureTypeCubeArray) slices *= 6;
	[self copyFromTexture:src sourceSlice:0 sourceLevel:0 toTexture:dst destinationSlice:0 destinationLevel:0
	           sliceCount:slices ? slices : 1 levelCount:levels ? levels : 1];
}
- (void)generateMipmapsForTexture:(id)texture
{
	if (texture == nil || [texture mipmapLevelCount] < 2) return;
	SweetNativeMetalWork *work = [SweetNativeMetalWork new];
	work->nativeBlit = 2; work->color[0] = texture;
	[self nativeRecord:work];
}
- (void)copyFromBuffer:(id)src sourceOffset:(NSUInteger)so toBuffer:(id)dst destinationOffset:(NSUInteger)dof size:(NSUInteger)size
{
	if (src == nil || dst == nil || size == 0) return;
	SweetNativeMetalWork *work = [SweetNativeMetalWork new];
	work->nativeBlit = 3; work->color[0] = src; work->color[1] = dst;
	work->blitSrcOffset = so; work->blitDstOffset = dof; work->blitBytes = size;
	[self nativeRecord:work];
}
- (void)fillBuffer:(id)buffer range:(NSRange)range value:(uint8_t)value
{
	if (buffer == nil || range.length == 0) return;
	SweetNativeMetalWork *work = [SweetNativeMetalWork new];
	work->nativeBlit = 4; work->color[0] = buffer;
	work->blitDstOffset = range.location; work->blitBytes = range.length; work->blitValue = value;
	[self nativeRecord:work];
}
/* Buffer <-> texture copies are not implemented yet: said once each, the command buffer goes on. */
- (void)copyFromBuffer:(id)src sourceOffset:(NSUInteger)so sourceBytesPerRow:(NSUInteger)bpr sourceBytesPerImage:(NSUInteger)bpi
            sourceSize:(MTLSize)size toTexture:(id)dst destinationSlice:(NSUInteger)ds destinationLevel:(NSUInteger)dl
     destinationOrigin:(MTLOrigin)dorg
{
	(void)src; (void)so; (void)bpr; (void)bpi; (void)size; (void)dst; (void)ds; (void)dl; (void)dorg;
	static unsigned told; if (told++ < 4) rw_print("blit: buffer -> texture copy is not implemented (dropped)\n");
}
- (void)copyFromBuffer:(id)src sourceOffset:(NSUInteger)so sourceBytesPerRow:(NSUInteger)bpr sourceBytesPerImage:(NSUInteger)bpi
            sourceSize:(MTLSize)size toTexture:(id)dst destinationSlice:(NSUInteger)ds destinationLevel:(NSUInteger)dl
     destinationOrigin:(MTLOrigin)dorg options:(NSUInteger)options
{
	(void)options;
	[self copyFromBuffer:src sourceOffset:so sourceBytesPerRow:bpr sourceBytesPerImage:bpi sourceSize:size toTexture:dst
	    destinationSlice:ds destinationLevel:dl destinationOrigin:dorg];
}
- (void)copyFromTexture:(id)src sourceSlice:(NSUInteger)ss sourceLevel:(NSUInteger)sl sourceOrigin:(MTLOrigin)so
             sourceSize:(MTLSize)size toBuffer:(id)dst destinationOffset:(NSUInteger)dof destinationBytesPerRow:(NSUInteger)bpr
destinationBytesPerImage:(NSUInteger)bpi
{
	(void)src; (void)ss; (void)sl; (void)so; (void)size; (void)dst; (void)dof; (void)bpr; (void)bpi;
	static unsigned told; if (told++ < 4) rw_print("blit: texture -> buffer copy is not implemented (dropped)\n");
}
- (void)copyFromTexture:(id)src sourceSlice:(NSUInteger)ss sourceLevel:(NSUInteger)sl sourceOrigin:(MTLOrigin)so
             sourceSize:(MTLSize)size toBuffer:(id)dst destinationOffset:(NSUInteger)dof destinationBytesPerRow:(NSUInteger)bpr
destinationBytesPerImage:(NSUInteger)bpi options:(NSUInteger)options
{
	(void)options;
	[self copyFromTexture:src sourceSlice:ss sourceLevel:sl sourceOrigin:so sourceSize:size toBuffer:dst destinationOffset:dof
	   destinationBytesPerRow:bpr destinationBytesPerImage:bpi];
}
@end
static id native_blit_encoder_new(id commandBuffer)
{
	SweetNativeMetalBlitEncoder *e = [[SweetNativeMetalBlitEncoder alloc] init];
	e->_cb = [commandBuffer retain];
	static unsigned told;
	if (told++ < 4) rw_print("blit encoder made (26c)\n");
	return [e autorelease];
}
